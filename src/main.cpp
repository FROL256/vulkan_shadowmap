#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#ifdef WIN32
#pragma comment(lib,"glfw3.lib")
#endif

#include <vulkan/vulkan.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <memory>

#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cassert>

#include "vk_utils.h"
#include "vk_geom.h"
#include "vk_copy.h"
#include "vk_texture.h"
#include "vk_quad.h"
#include "Bitmap.h"

#include "Camera.h"

const int WIDTH  = 1024;
const int HEIGHT = 1024;

const int MAX_FRAMES_IN_FLIGHT = 1;

const std::vector<const char*> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct g_input_t
{
  bool firstMouse   = true;
  bool captureMouse = false;
  bool capturedMouseJustNow = false;
  bool drawFSQuad   = false;
  bool controlLight = false;

  float lastX,lastY, scrollY;
  float camMoveSpeed     = 1.0f;
  float mouseSensitivity = 0.1f;

} g_input;

void OnKeyboardPressed(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	switch (key)
	{
	case GLFW_KEY_ESCAPE: 
		if (action == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GL_TRUE);
		break;
  case GLFW_KEY_LEFT_SHIFT:
    g_input.camMoveSpeed = 10.0f;
  break;

	case GLFW_KEY_SPACE: 
		break;
  case GLFW_KEY_1:
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    break;
  case GLFW_KEY_2:
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    break;

  case GLFW_KEY_9:
    g_input.drawFSQuad = true;
    break;  

  case GLFW_KEY_0:
    g_input.drawFSQuad = false;
    break;  

  case GLFW_KEY_L:
    g_input.controlLight = true;
    break;  

  case GLFW_KEY_C:
    g_input.controlLight = false;
    break;  
	
  default:
		//if (action == GLFW_PRESS)
		//	keys[key] = true;
		//else if (action == GLFW_RELEASE)
		//	keys[key] = false;
    break;  
	}
}

void OnMouseButtonClicked(GLFWwindow* window, int button, int action, int mods)
{
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    g_input.captureMouse = !g_input.captureMouse;
  
  if (g_input.captureMouse)
  {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    g_input.capturedMouseJustNow = true;
  }
  else
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

}

void OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
  if (g_input.firstMouse)
  {
    g_input.lastX      = float(xpos);
    g_input.lastY      = float(ypos);
    g_input.firstMouse = false;
  }

  float xoffset = float(xpos) - g_input.lastX;
  float yoffset = g_input.lastY - float(ypos);  
  
  g_input.lastX = float(xpos);
  g_input.lastY = float(ypos);
}

void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset)
{
  g_input.scrollY = float(yoffset);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class HelloTriangleApplication 
{
public:

  void run() 
  {
    InitWindow();
    
    InitVulkan();
    CreateResources();

    MainLoop();

    Cleanup();
  }

private:
  GLFWwindow * window;

  VkInstance instance;
  std::vector<const char*> enabledLayers;

  VkDebugUtilsMessengerEXT debugMessenger;
  VkSurfaceKHR surface;

  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device;

  VkQueue graphicsQueue, presentQueue, transferQueue;

  vk_utils::ScreenBufferResources screen;
  VkImage        depthImage       = nullptr;
  VkDeviceMemory depthImageMemory = nullptr;
  VkImageView    depthImageView   = nullptr;

  VkRenderPass     renderPass             = nullptr;
  VkPipelineLayout pipelineLayout         = nullptr;
  VkPipeline       graphicsPipeline       = nullptr;
  VkPipeline       graphicsPipelineShadow = nullptr;

  VkCommandPool                commandPool;
  std::vector<VkCommandBuffer> commandBuffers;

  VkDeviceMemory        m_memAllMeshes   = nullptr; //
  VkDeviceMemory        m_memAllTextures = nullptr;
  VkDeviceMemory        m_memShadowMap   = nullptr;

  struct SyncObj
  {
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence>     inFlightFences;
  } m_sync;

  size_t currentFrame = 0;
  Camera m_cam;
  struct ShadowMapCam
  {
    ShadowMapCam() 
    {  
      cam.pos    = LiteMath::float3(10.0f, 10.0f, 10.0f);
      cam.lookAt = LiteMath::float3(0, 0, 0);
      cam.up     = LiteMath::float3(0, 1, 0);
  
      radius          = 5.0f;
      lightTargetDist = 20.0f;
    }
  
    void UpdatePlaneEquation()
    {
      planeEquation   = LiteMath::to_float4(cam.forward(), 0.0f); // A = n.x; B = n.y; C = n.z;
      planeEquation.w = -LiteMath::dot(cam.pos, cam.forward());   // D = -(A*p.x + B*p.y + C*P.z);
    }

    float  radius;
    float  lightTargetDist;
    Camera cam;
    LiteMath::float4 planeEquation;
  
  } m_light;
  
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  struct CopyEngine : public vk_geom::ICopyEngine, 
                      public vk_texture::ICopyEngine
  {
    CopyEngine(VkPhysicalDevice a_physicalDevice, VkDevice a_device, VkQueue a_transferQueue, size_t a_stagingBuffSize) : m_helper(a_physicalDevice, a_device, a_transferQueue, a_stagingBuffSize) {}
  
    void UpdateBuffer(VkBuffer a_dst, size_t a_dstOffset, const void* a_src, size_t a_size)    override { m_helper.UpdateBuffer(a_dst, a_dstOffset, a_src, a_size); }
    void UpdateImage(VkImage a_image, const void* a_src, int a_width, int a_height, int a_bpp) override { m_helper.UpdateImage(a_image, a_src, a_width, a_height, a_bpp); }

    VkCommandBuffer CmdBuffer() { return m_helper.CmdBuffer(); }

    vk_copy::SimpleCopyHelper m_helper;
  };


  std::unique_ptr<CopyEngine>       m_pCopyHelper;
  std::shared_ptr<vk_geom::IMesh>   m_pTerrainMesh;
  std::shared_ptr<vk_geom::IMesh>   m_pTeapotMesh;
  std::shared_ptr<vk_geom::IMesh>   m_pBunnyMesh;
  std::shared_ptr<vk_utils::FSQuad> m_pFSQuad;

  enum {TERRAIN_TEX = 0, STONE_TEX = 1, METAL_TEX = 2, TEXTURES_NUM = 3, // 
        SHADOW_MAP = 3, DESCRIPTORS_NUM = TEXTURES_NUM+1};               //

  std::shared_ptr<vk_texture::SimpleTexture2D>     m_pTex[TEXTURES_NUM];
  std::shared_ptr<vk_texture::RenderableTexture2D> m_pShadowMap;

  // Descriptors represent resources in shaders. They allow us to use things like
  // uniform buffers, storage buffers and images in GLSL.
  // A single descriptor represents a single resource, and several descriptors are organized
  // into descriptor sets, which are basically just collections of descriptors.
  // 
  VkDescriptorPool      descriptorPool = nullptr;
  VkDescriptorSet       descriptorSet[DESCRIPTORS_NUM] = {}; // for our textures
  VkDescriptorSet       descriptorSetWithSM[TEXTURES_NUM] = {};
  VkDescriptorSetLayout descriptorSetLayout = nullptr, descriptorSetLayoutSM = nullptr;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  void InitWindow() 
  {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    glfwSetKeyCallback        (window, OnKeyboardPressed);  
	  glfwSetCursorPosCallback  (window, OnMouseMove); 
    glfwSetMouseButtonCallback(window, OnMouseButtonClicked);
	  glfwSetScrollCallback     (window, OnMouseScroll);
	  //glfwSetInputMode          (window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // capture cursor at start
    glfwSwapInterval(0);
  }

  void UpdateCamera(Camera& a_cam, float secondsElapsed)
  {
    //move position of camera based on WASD keys, and XZ keys for up and down
    if (glfwGetKey(window, 'S'))
      a_cam.offsetPosition(secondsElapsed * g_input.camMoveSpeed * -a_cam.forward());
    else if (glfwGetKey(window, 'W'))
      a_cam.offsetPosition(secondsElapsed * g_input.camMoveSpeed * a_cam.forward());
    
    if (glfwGetKey(window, 'A'))
      a_cam.offsetPosition(secondsElapsed * g_input.camMoveSpeed * -a_cam.right());
    else if (glfwGetKey(window, 'D'))
      a_cam.offsetPosition(secondsElapsed * g_input.camMoveSpeed * a_cam.right());
    
    if (glfwGetKey(window, 'F'))
      a_cam.offsetPosition(secondsElapsed * g_input.camMoveSpeed * -a_cam.up);
    else if (glfwGetKey(window, 'R'))
      a_cam.offsetPosition(secondsElapsed * g_input.camMoveSpeed * a_cam.up);
    
    //rotate camera based on mouse movement
    //
    if (g_input.captureMouse)
    {
      if(g_input.capturedMouseJustNow)
        glfwSetCursorPos(window, 0, 0);
    
      double mouseX, mouseY;
      glfwGetCursorPos(window, &mouseX, &mouseY);
      a_cam.offsetOrientation(g_input.mouseSensitivity * float(mouseY), g_input.mouseSensitivity * float(mouseX));
      glfwSetCursorPos(window, 0, 0); //reset the mouse, so it doesn't go out of the window
      g_input.capturedMouseJustNow = false;
    }
    
    //increase or decrease field of view based on mouse wheel
    //
    const float zoomSensitivity = -0.2f;
    float fieldOfView = a_cam.fov + zoomSensitivity * (float)g_input.scrollY;
    if(fieldOfView < 1.0f) fieldOfView   = 1.0f;
    if(fieldOfView > 180.0f) fieldOfView = 180.0f;
    a_cam.fov       = fieldOfView;

    g_input.scrollY = 0.0f;
  }


  static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFn(
    VkDebugReportFlagsEXT                       flags,
    VkDebugReportObjectTypeEXT                  objectType,
    uint64_t                                    object,
    size_t                                      location,
    int32_t                                     messageCode,
    const char*                                 pLayerPrefix,
    const char*                                 pMessage,
    void*                                       pUserData)
  {
    printf("[Debug Report]: %s: %s\n", pLayerPrefix, pMessage);
    return VK_FALSE;
  }
  VkDebugReportCallbackEXT debugReportCallback;
  

  void InitVulkan() 
  {
    const int deviceId = 1;

    std::vector<const char*> extensions;
    {
      uint32_t glfwExtensionCount = 0;
      const char** glfwExtensions;
      glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
      extensions     = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
    }

    instance = vk_utils::CreateInstance(enableValidationLayers, enabledLayers, extensions);
    if (enableValidationLayers)
      vk_utils::InitDebugReportCallback(instance, &debugReportCallbackFn, &debugReportCallback);

    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
      throw std::runtime_error("glfwCreateWindowSurface: failed to create window surface!");
  
    physicalDevice    = vk_utils::FindPhysicalDevice(instance, true, deviceId);
    auto queueFID     = vk_utils::GetQueueFamilyIndex(physicalDevice, VK_QUEUE_GRAPHICS_BIT);
    auto queueCopyFID = vk_utils::GetQueueFamilyIndex(physicalDevice, VK_QUEUE_TRANSFER_BIT);

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFID, surface, &presentSupport);
    if (!presentSupport)
      throw std::runtime_error("vkGetPhysicalDeviceSurfaceSupportKHR: no present support for the target device and graphics queue");

    device = vk_utils::CreateLogicalDevice(queueFID, physicalDevice, enabledLayers, deviceExtensions);
    vkGetDeviceQueue(device, queueFID,     0, &graphicsQueue);
    vkGetDeviceQueue(device, queueFID,     0, &presentQueue);
    vkGetDeviceQueue(device, queueCopyFID, 0, &transferQueue);
    
    // ==> commadnPool
    {
      VkCommandPoolCreateInfo poolInfo = {};
      poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      poolInfo.queueFamilyIndex = vk_utils::GetQueueFamilyIndex(physicalDevice, VK_QUEUE_GRAPHICS_BIT);

      if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("[CreateCommandPoolAndBuffers]: failed to create command pool!");
    }

    vk_utils::CreateCwapChain(physicalDevice, device, surface, WIDTH, HEIGHT,
                              &screen);

    vk_utils::CreateScreenImageViews(device, &screen);

    m_pCopyHelper = std::make_unique<CopyEngine>(physicalDevice, device, transferQueue, 64*1024*1024);
  }

  void CreateResources()
  {
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    vk_utils::CreateDepthTexture(device, physicalDevice, WIDTH, HEIGHT, 
                                 &depthImageMemory, &depthImage, &depthImageView);

    CreateRenderPass(device, screen.swapChainImageFormat, 
                     &renderPass);
  
    CreateScreenFrameBuffers(device, renderPass, depthImageView, &screen);

    m_pFSQuad = std::make_shared<vk_utils::FSQuad>();
    m_pFSQuad->Create(device, "shaders/quad_vert.spv", "shaders/quad_frag.spv", 
                      vk_utils::RenderTargetInfo2D{ VkExtent2D{ WIDTH, HEIGHT }, screen.swapChainImageFormat, 
                                                    VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR });
  
    CreateSyncObjects(device, &m_sync);

    CreateDescriptorSetLayoutForSingleTexture(device, &descriptorSetLayout);
    CreateDescriptorSetLayoutForTextureAndShadowMap(device, &descriptorSetLayoutSM);

    // create textures
    //
    int  w1, h1, w2, h2, w3, h3;
    auto data1 = LoadBMP("data/texture1.bmp",   &w1, &h1);
    auto data2 = LoadBMP("data/stonebrick.bmp", &w2, &h2);
    auto data3 = LoadBMP("data/metal.bmp",      &w3, &h3);

    if (data1.size() == 0)
      RUN_TIME_ERROR("data/texture1.bmp | NOT FOUND!");
    if (data2.size() == 0)
      RUN_TIME_ERROR("data/stonebrick.bmp | NOT FOUND!");
    if (data3.size() == 0)
      RUN_TIME_ERROR("data/metal.bmp | NOT FOUND!");

    for(int i=0;i<TEXTURES_NUM;i++)
      m_pTex[i] = std::make_shared<vk_texture::SimpleTexture2D>();
    m_pShadowMap = std::make_shared<vk_texture::RenderableTexture2D>();
   
    auto memReqTex1 = m_pTex[TERRAIN_TEX]->CreateImage(device, w1, h1, VK_FORMAT_R8G8B8A8_UNORM);
    auto memReqTex2 = m_pTex[STONE_TEX]->CreateImage(device, w2, h2, VK_FORMAT_R8G8B8A8_UNORM);
    auto memReqTex3 = m_pTex[METAL_TEX]->CreateImage(device, w3, h3, VK_FORMAT_R8G8B8A8_UNORM);
    auto memReqTex4 = m_pShadowMap->CreateImage(device, 2048, 2048, VK_FORMAT_D16_UNORM);

    assert(memReqTex1.memoryTypeBits == memReqTex2.memoryTypeBits);
    assert(memReqTex1.memoryTypeBits == memReqTex3.memoryTypeBits);
    //assert(memReqTex1.memoryTypeBits == memReqTex4.memoryTypeBits); // well, usually not, please make separate allocation for shadow map :)
    
    // memory for all read-only textures
    {
      VkMemoryAllocateInfo allocateInfo = {};
      allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocateInfo.pNext           = nullptr;
      allocateInfo.allocationSize  = memReqTex1.size + memReqTex2.size + memReqTex3.size;
      allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(memReqTex1.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physicalDevice);

      VK_CHECK_RESULT(vkAllocateMemory(device, &allocateInfo, NULL, &m_memAllTextures));
    }

    // memory for all shadowmaps (well, if you have them more than 1 ...)
    {
      VkMemoryAllocateInfo allocateInfo = {};
      allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocateInfo.pNext           = nullptr;
      allocateInfo.allocationSize  = memReqTex4.size;
      allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(memReqTex4.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physicalDevice);

      VK_CHECK_RESULT(vkAllocateMemory(device, &allocateInfo, NULL, &m_memShadowMap));
    }

    m_pTex[0]->BindMemory(m_memAllTextures, 0);
    m_pTex[1]->BindMemory(m_memAllTextures, memReqTex1.size);
    m_pTex[2]->BindMemory(m_memAllTextures, memReqTex1.size + memReqTex2.size);
    m_pShadowMap->BindMemory(m_memShadowMap, 0);

    VkSampler samplers[] = { m_pTex[0]->Sampler(), m_pTex[1]->Sampler(), m_pTex[2]->Sampler(), m_pShadowMap->Sampler()};
    VkImageView views [] = { m_pTex[0]->View(),    m_pTex[1]->View(),    m_pTex[2]->View(),    m_pShadowMap->View() };

    CreateDescriptorSetsForImages(device, descriptorSetLayout, samplers, views, DESCRIPTORS_NUM,
                                  &descriptorPool, descriptorSet);

    CreateDescriptorSetsForImagesWithSM(device, descriptorSetLayoutSM, descriptorPool, samplers, views, m_pShadowMap->Sampler(), m_pShadowMap->View(), TEXTURES_NUM,
                                        descriptorSetWithSM);

    m_pTex[TERRAIN_TEX]->Update(data1.data(), w1, h1, sizeof(int), m_pCopyHelper.get()); // --> put m_pTex[i] in transfer_dst layout
    m_pTex[STONE_TEX]->Update(data2.data(), w2, h2, sizeof(int), m_pCopyHelper.get());   // --> put m_pTex[i] in transfer_dst layout
    m_pTex[METAL_TEX]->Update(data3.data(), w3, h3, sizeof(int), m_pCopyHelper.get());   // --> put m_pTex[i] in transfer_dst layout
    
    // generate all mips
    //
    {
      VkCommandBuffer cmdBuff = m_pCopyHelper->CmdBuffer(); 

      vkResetCommandBuffer(cmdBuff, 0);
      VkCommandBufferBeginInfo beginInfo = {};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
      if (vkBeginCommandBuffer(cmdBuff, &beginInfo) != VK_SUCCESS) 
         throw std::runtime_error("[FFF]: failed to begin command buffer!");
      
      for (int i = 0; i<TEXTURES_NUM; i++)
        m_pTex[i]->GenerateMipsCmd(cmdBuff);                                            // --> put m_pTex[i] in shader_read layout
     
      vkEndCommandBuffer(cmdBuff);

      vk_utils::ExecuteCommandBufferNow(cmdBuff, transferQueue, device);
    }

    // create meshes
    //
    m_pTerrainMesh = std::make_shared< vk_geom::CompactMesh_T3V4x2F >();
    m_pTeapotMesh  = std::make_shared< vk_geom::CompactMesh_T3V4x2F >();
    m_pBunnyMesh   = std::make_shared< vk_geom::CompactMesh_T3V4x2F >();

    auto meshData  = cmesh::CreateQuad(64, 64, 4.0f);
    auto teapData  = cmesh::LoadMeshFromVSGF("data/teapot.vsgf");
    auto bunnyData = cmesh::LoadMeshFromVSGF("data/bunny0.vsgf");

    if(teapData.VerticesNum() == 0)
      RUN_TIME_ERROR("can't load mesh at 'data/teapot.vsgf'");

    auto memReq1 = m_pTerrainMesh->CreateBuffers(device, int(meshData.VerticesNum()), int(meshData.IndicesNum())); // what if memReq1 and memReq2 differs in memoryTypeBits ... ? )
    auto memReq2 = m_pTeapotMesh->CreateBuffers (device, int(teapData.VerticesNum()), int(teapData.IndicesNum())); //
    auto memReq3 = m_pBunnyMesh->CreateBuffers  (device, int(bunnyData.VerticesNum()), int(bunnyData.IndicesNum())); //

    assert(memReq1.memoryTypeBits == memReq2.memoryTypeBits); // assume this in our simple demo
    assert(memReq1.memoryTypeBits == memReq3.memoryTypeBits); // assume this in our simple demo

    // allocate memory for all meshes
    //
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext           = nullptr;
    allocateInfo.allocationSize  = memReq1.size + memReq2.size + memReq3.size; // specify required memory size
    allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(memReq1.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physicalDevice); 
    
    VK_CHECK_RESULT(vkAllocateMemory(device, &allocateInfo, NULL, &m_memAllMeshes));

    m_pTerrainMesh->BindBuffers(m_memAllMeshes, 0);
    m_pTeapotMesh->BindBuffers (m_memAllMeshes, memReq1.size);
    m_pBunnyMesh->BindBuffers  (m_memAllMeshes, memReq1.size + memReq2.size);

    m_pTerrainMesh->UpdateBuffers(meshData, m_pCopyHelper.get());
    m_pTeapotMesh->UpdateBuffers (teapData, m_pCopyHelper.get());
    m_pBunnyMesh->UpdateBuffers  (bunnyData, m_pCopyHelper.get()); 

    assert(m_pShadowMap   != nullptr);
    assert(m_pTerrainMesh != nullptr);

    CreateGraphicsPipelines(device, screen.swapChainExtent, renderPass, m_pShadowMap, descriptorSetLayoutSM, m_pTerrainMesh->VertexInputLayout(),
                            &pipelineLayout, &graphicsPipeline, &graphicsPipelineShadow);

    // create command buffers for rendering
    {
      commandBuffers.resize(screen.swapChainFramebuffers.size());

      VkCommandBufferAllocateInfo allocInfo = {};
      allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocInfo.commandPool        = commandPool;
      allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

      if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("[CreateCommandPoolAndBuffers]: failed to allocate command buffers!");
    }

  }


  void MainLoop()
  {
    constexpr int NAverage = 60;
    double avgTime = 0.0;
    int avgCounter = 0;

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) 
    {
      double thisTime = glfwGetTime();
      float diffTime  = float(thisTime - lastTime);
      lastTime        = thisTime;
      
      if(g_input.controlLight)
        UpdateCamera(m_light.cam, diffTime);
      else
        UpdateCamera(m_cam, diffTime);
      
      glfwPollEvents();
      DrawFrame();

      // count and print FPS
      //
      avgTime += diffTime;
      avgCounter++;
      if(avgCounter >= NAverage)
      {
        std::stringstream strout;
        strout << "FPS = " << int( 1.0/(avgTime/double(NAverage)) );
        glfwSetWindowTitle(window, strout.str().c_str());
        avgTime    = 0.0;
        avgCounter = 0;
      }
      
    }

    vkDeviceWaitIdle(device);
  }

  void Cleanup() 
  { 
    m_pShadowMap = nullptr;
    for(int i=0;i<TEXTURES_NUM;i++)
      m_pTex[i] = nullptr;    // smart pointer will destroy resources
  
    m_pCopyHelper  = nullptr; // smart pointer will destroy resources
    m_pTerrainMesh = nullptr; // smart pointer will destroy resources
    m_pTeapotMesh  = nullptr;
    m_pBunnyMesh   = nullptr;
    m_pFSQuad      = nullptr;

    // free our vbos
    vkFreeMemory(device, m_memAllMeshes, NULL);

    if(m_memAllTextures != nullptr)
      vkFreeMemory(device, m_memAllTextures, NULL);

    if(m_memShadowMap != nullptr)
      vkFreeMemory(device, m_memShadowMap, NULL);

    if(depthImageMemory != nullptr)
    {
      vkFreeMemory      (device, depthImageMemory, NULL);
      vkDestroyImageView(device, depthImageView, NULL);
      vkDestroyImage    (device, depthImage, NULL);
    }

    if (descriptorPool != nullptr)
      vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    if (descriptorSetLayout != nullptr)
    {
      vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
      vkDestroyDescriptorSetLayout(device, descriptorSetLayoutSM, NULL);
    }

    if (enableValidationLayers)
    {
      // destroy callback.
      auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
      if (func == nullptr)
        throw std::runtime_error("Could not load vkDestroyDebugReportCallbackEXT");
      func(instance, debugReportCallback, NULL);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
    {
      vkDestroySemaphore(device, m_sync.renderFinishedSemaphores[i], nullptr);
      vkDestroySemaphore(device, m_sync.imageAvailableSemaphores[i], nullptr);
      vkDestroyFence    (device, m_sync.inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);

    for (auto framebuffer : screen.swapChainFramebuffers) {
      vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    if(graphicsPipelineShadow != nullptr)
      vkDestroyPipeline(device, graphicsPipelineShadow, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass    (device, renderPass, nullptr);

    for (auto imageView : screen.swapChainImageViews) {
      vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, screen.swapChain, nullptr);
    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();
  }

  static void CreateDescriptorSetLayoutForSingleTexture(VkDevice a_device, VkDescriptorSetLayout *a_pDSLayout)
  {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[1];
  
    descriptorSetLayoutBinding[0].binding            = 0;
    descriptorSetLayoutBinding[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBinding[0].descriptorCount    = 1;
    descriptorSetLayoutBinding[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBinding[0].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings    = descriptorSetLayoutBinding;

    // Create the descriptor set layout.
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(a_device, &descriptorSetLayoutCreateInfo, NULL, a_pDSLayout));
  }

  static void CreateDescriptorSetLayoutForTextureAndShadowMap(VkDevice a_device, VkDescriptorSetLayout *a_pDSLayout)
  {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[2];

    descriptorSetLayoutBinding[0].binding            = 0;
    descriptorSetLayoutBinding[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBinding[0].descriptorCount    = 1;
    descriptorSetLayoutBinding[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBinding[0].pImmutableSamplers = nullptr;

    descriptorSetLayoutBinding[1].binding            = 1;
    descriptorSetLayoutBinding[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBinding[1].descriptorCount    = 1;
    descriptorSetLayoutBinding[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBinding[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 2;
    descriptorSetLayoutCreateInfo.pBindings    = descriptorSetLayoutBinding;

    // Create the descriptor set layout.
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(a_device, &descriptorSetLayoutCreateInfo, NULL, a_pDSLayout));
  }

  void CreateDescriptorSetsForImages(VkDevice a_device, const VkDescriptorSetLayout a_dsLayout, VkSampler* a_samplers, VkImageView *a_views, int a_imagesNumber,
                                     VkDescriptorPool* a_pDSPool, VkDescriptorSet* a_pDS)
  {
    assert(a_pDSPool   != nullptr);
    assert(a_pDS       != nullptr);

    assert(a_samplers != nullptr);
    assert(a_views    != nullptr);
    assert(a_imagesNumber != 0);

    std::vector<VkDescriptorPoolSize> descriptorPoolSize(a_imagesNumber*2);
    for(int i=0;i<descriptorPoolSize.size();i++)
    {
      descriptorPoolSize[i].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorPoolSize[i].descriptorCount = 2;
    }
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets       = a_imagesNumber*2; // we need to allocate at least 1 descriptor set
    descriptorPoolCreateInfo.poolSizeCount = uint32_t(descriptorPoolSize.size());
    descriptorPoolCreateInfo.pPoolSizes    = descriptorPoolSize.data();

    VK_CHECK_RESULT(vkCreateDescriptorPool(a_device, &descriptorPoolCreateInfo, NULL, a_pDSPool));

    // With the pool allocated, we can now allocate the descriptor set.
    //
    std::vector<VkDescriptorSetLayout> layouts(a_imagesNumber);
    for(int i=0;i<layouts.size();i++)
      layouts[i] = a_dsLayout;

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool     = (*a_pDSPool);   // pool to allocate from.
    descriptorSetAllocateInfo.descriptorSetCount = a_imagesNumber; // allocate a descriptor set for buffer and image
    descriptorSetAllocateInfo.pSetLayouts        = layouts.data();

    // allocate descriptor set.
    VK_CHECK_RESULT(vkAllocateDescriptorSets(a_device, &descriptorSetAllocateInfo, a_pDS));

    // Next, we need to connect our actual texture with the descriptor.
    // We use vkUpdateDescriptorSets() to update the descriptor set.
    //
    
    std::vector<VkDescriptorImageInfo> descriptorImageInfos(a_imagesNumber);
    std::vector<VkWriteDescriptorSet>  writeDescriptorSets (a_imagesNumber);

    for(int imageId = 0; imageId < a_imagesNumber; imageId++)
    {
      descriptorImageInfos[imageId]             = VkDescriptorImageInfo{};
      descriptorImageInfos[imageId].sampler     = a_samplers[imageId];
      descriptorImageInfos[imageId].imageView   = a_views   [imageId];
      descriptorImageInfos[imageId].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      writeDescriptorSets[imageId]                 = VkWriteDescriptorSet{};
      writeDescriptorSets[imageId].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeDescriptorSets[imageId].dstSet          = a_pDS[imageId]; 
      writeDescriptorSets[imageId].dstBinding      = 0;              
      writeDescriptorSets[imageId].descriptorCount = 1;              
      writeDescriptorSets[imageId].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // image.
      writeDescriptorSets[imageId].pImageInfo      = descriptorImageInfos.data() + imageId;
    }

    vkUpdateDescriptorSets(a_device, a_imagesNumber, writeDescriptorSets.data(), 0, NULL);
  }

  void CreateDescriptorSetsForImagesWithSM(VkDevice a_device, const VkDescriptorSetLayout a_dsLayout, VkDescriptorPool a_dsPool,
                                           VkSampler* a_samplers, VkImageView *a_views, VkSampler a_shadowSampler, VkImageView a_view,
                                           int a_imagesNumber, VkDescriptorSet* a_pDS)
  {
    assert(a_pDS != nullptr);

    assert(a_samplers != nullptr);
    assert(a_views != nullptr);
    assert(a_imagesNumber != 0);

    // With the pool allocated, we can now allocate the descriptor set.
    //
    std::vector<VkDescriptorSetLayout> layouts(a_imagesNumber);
    for (int i = 0; i<layouts.size(); i++)
      layouts[i] = a_dsLayout;

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool     = a_dsPool;   // pool to allocate from.
    descriptorSetAllocateInfo.descriptorSetCount = a_imagesNumber; // allocate a descriptor set for buffer and image
    descriptorSetAllocateInfo.pSetLayouts        = layouts.data();

    // allocate descriptor set.
    VK_CHECK_RESULT(vkAllocateDescriptorSets(a_device, &descriptorSetAllocateInfo, a_pDS));

    // Next, we need to connect our actual texture with the descriptor.
    // We use vkUpdateDescriptorSets() to update the descriptor set.
    //
    std::vector<VkDescriptorImageInfo> descriptorImageInfos(a_imagesNumber);
    std::vector<VkWriteDescriptorSet>  writeDescriptorSets(a_imagesNumber);

    VkDescriptorImageInfo shadowImageInfo;
    shadowImageInfo             = VkDescriptorImageInfo{};
    shadowImageInfo.sampler     = a_shadowSampler;
    shadowImageInfo.imageView   = a_view;
    shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    for (int imageId = 0; imageId < a_imagesNumber; imageId++)
    {
      descriptorImageInfos[imageId]             = VkDescriptorImageInfo{};
      descriptorImageInfos[imageId].sampler     = a_samplers[imageId];
      descriptorImageInfos[imageId].imageView   = a_views[imageId];
      descriptorImageInfos[imageId].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      VkDescriptorImageInfo descrImageInfos[2] = { descriptorImageInfos[imageId] , shadowImageInfo };

      writeDescriptorSets[imageId]                 = VkWriteDescriptorSet{};
      writeDescriptorSets[imageId].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeDescriptorSets[imageId].dstSet          = a_pDS[imageId];
      writeDescriptorSets[imageId].dstBinding      = 0;
      writeDescriptorSets[imageId].descriptorCount = 2;
      writeDescriptorSets[imageId].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // image.
      writeDescriptorSets[imageId].pImageInfo      = descrImageInfos;

      vkUpdateDescriptorSets(a_device, 1, writeDescriptorSets.data() + imageId, 0, NULL);
    }

  }


  static void CreateRenderPass(VkDevice a_device, VkFormat a_swapChainImageFormat, 
                               VkRenderPass* a_pRenderPass)
  {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format         = a_swapChainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // add depth test
    //
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass    = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   
    VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo  = {};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments    = &attachments[0];
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(a_device, &renderPassInfo, nullptr, a_pRenderPass) != VK_SUCCESS)
      throw std::runtime_error("[CreateRenderPass]: failed to create render pass!");
  }

  static void CreateGraphicsPipelines(VkDevice a_device, VkExtent2D a_screenExtent, VkRenderPass a_renderPass, std::shared_ptr<vk_texture::RenderableTexture2D> a_shadowTex,
                                      VkDescriptorSetLayout a_dslayout, VkPipelineVertexInputStateCreateInfo a_vertLayout,
                                      VkPipelineLayout* a_pLayout, VkPipeline* a_pPipeline, VkPipeline* a_pPipelineShadow)
  {
    const char* vs_path = "shaders/cmesh_t3v4x2.spv";
    const char* ps_path = "shaders/direct_light.spv";

    auto vertShaderCode = vk_utils::ReadFile(vs_path);
    auto fragShaderCode = vk_utils::ReadFile(ps_path);

    VkShaderModule vertShaderModule = vk_utils::CreateShaderModule(a_device, vertShaderCode);
    VkShaderModule fragShaderModule = vk_utils::CreateShaderModule(a_device, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x          = 0.0f;
    viewport.y          = 0.0f;
    viewport.width      = (float)a_screenExtent.width;
    viewport.height     = (float)a_screenExtent.height;
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = a_screenExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL; // VK_POLYGON_MODE_FILL; // VK_POLYGON_MODE_LINE
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.logicOp           = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPushConstantRange pcRange = {};   
    pcRange.stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    pcRange.offset     = 0;
    pcRange.size       = 16*3*sizeof(float);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pcRange;
    pipelineLayoutInfo.pSetLayouts            = &a_dslayout; //&descriptorSetLayout;
    pipelineLayoutInfo.setLayoutCount         = 1;

    if (vkCreatePipelineLayout(a_device, &pipelineLayoutInfo, nullptr, a_pLayout) != VK_SUCCESS)
      throw std::runtime_error("[CreateGraphicsPipeline]: failed to create pipeline layout!");

    VkPipelineDepthStencilStateCreateInfo depthStencilTest = {};
    depthStencilTest.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilTest.depthTestEnable       = true;  
    depthStencilTest.depthWriteEnable      = true;
    depthStencilTest.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencilTest.depthBoundsTestEnable = false;
    depthStencilTest.stencilTestEnable     = false;
    depthStencilTest.depthBoundsTestEnable = VK_FALSE;
    depthStencilTest.minDepthBounds        = 0.0f; // Optional
    depthStencilTest.maxDepthBounds        = 1.0f; // Optional

    auto vertexInputInfo = a_vertLayout;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.flags               = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT; // we need this for creating derivative pipeline for shadowmap
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = (*a_pLayout);
    pipelineInfo.renderPass          = a_renderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.pDepthStencilState  = &depthStencilTest;

    if (vkCreateGraphicsPipelines(a_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, a_pPipeline) != VK_SUCCESS)
      throw std::runtime_error("[CreateGraphicsPipeline]: failed to create graphics pipeline!");

    vkDestroyShaderModule(a_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(a_device, vertShaderModule, nullptr);

    // create similar pipeline for shadow map. But now we do not attach pixel shader.
    //
    CreateDerivedPipeline(a_device, pipelineInfo, (*a_pPipeline), 
                          a_shadowTex->RenderPass(), uint32_t(a_shadowTex->Width()), uint32_t(a_shadowTex->Height()), vs_path, nullptr,
                          a_pPipelineShadow);
  }

  static void CreateDerivedPipeline(VkDevice a_device, VkGraphicsPipelineCreateInfo a_pipelineInfo, VkPipeline a_basePipeline, 
                                    VkRenderPass a_renderPass, uint32_t a_width, uint32_t a_height, const char* a_vsPath, const char* a_fsPath,
                                    VkPipeline* a_pDerivedPipeline)
  {
    std::vector<uint32_t> vertShaderCode, fragShaderCode;
    VkShaderModule vertShaderModule = nullptr, fragShaderModule = nullptr;

    assert(a_vsPath != nullptr);
    {
      vertShaderCode   = vk_utils::ReadFile(a_vsPath);
      vertShaderModule = vk_utils::CreateShaderModule(a_device, vertShaderCode);
    }

    if (a_fsPath != nullptr) // when render to shadowmap fragment shader could be nullptr
    {
      fragShaderCode   = vk_utils::ReadFile(a_fsPath);
      fragShaderModule = vk_utils::CreateShaderModule(a_device, fragShaderCode);
    }

    // Modify pipeline info to reflect derivation
    //
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkViewport viewport = {};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = +(float)a_width;
    viewport.height   = +(float)a_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset   = { 0, 0 };
    scissor.extent   = VkExtent2D{ a_width, a_height };

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    a_pipelineInfo.flags              = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    a_pipelineInfo.basePipelineHandle = a_basePipeline;
    a_pipelineInfo.basePipelineIndex  = -1;
    a_pipelineInfo.stageCount         = (a_fsPath == nullptr) ? 1 : 2;
    a_pipelineInfo.pStages            = shaderStages;
    a_pipelineInfo.renderPass         = a_renderPass;
    a_pipelineInfo.pViewportState     = &viewportState;

    if (vkCreateGraphicsPipelines(a_device, VK_NULL_HANDLE, 1, &a_pipelineInfo, nullptr, a_pDerivedPipeline) != VK_SUCCESS)
      throw std::runtime_error("[CreateDerivedPipeline]: failed to create graphics pipeline!");

    vkDestroyShaderModule(a_device, vertShaderModule, nullptr);
    if(fragShaderModule != nullptr)
      vkDestroyShaderModule(a_device, fragShaderModule, nullptr);
  }

  
  void DrawSceneCmd(VkCommandBuffer a_cmdBuff, LiteMath::float4x4 a_mWorldViewProj, LiteMath::float4x4 a_lightMatrix,
                    bool a_drawToShadowMap)
  {
    LiteMath::float3 a_lightDir = LiteMath::normalize(m_light.cam.pos - m_light.cam.lookAt);
    LiteMath::float4 a_planeEq  = m_light.planeEquation;
    auto             a_layout   = pipelineLayout;

    // draw plane/terrain
    //
    LiteMath::float4x4 matrices[3];

    {
      matrices[2].row[0] = LiteMath::to_float4(m_cam.pos, 0.0f);
      matrices[2].row[1] = LiteMath::to_float4(a_lightDir, 2.0f*m_light.lightTargetDist);
      matrices[2].row[2] = a_planeEq;
    }

    {
      auto mrot   = LiteMath::rotate_X_4x4(LiteMath::DEG_TO_RAD*90.0f);
      auto mWVP   = LiteMath::mul(a_mWorldViewProj, mrot);
      matrices[0] = LiteMath::transpose(mWVP);
      matrices[1] = a_lightMatrix;
    }

    if (!a_drawToShadowMap)
    {
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_layout, 0, 1, descriptorSetWithSM + TERRAIN_TEX, 0, NULL);
      vkCmdPushConstants(a_cmdBuff, a_layout, (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(float) * 3 * 16, matrices);
      m_pTerrainMesh->DrawCmd(a_cmdBuff);
    }

    // draw teapot
    {
      auto mtranslate = LiteMath::translate4x4({ -0.5f, 0.4f, -0.5f });
      auto mWVP       = LiteMath::mul(a_mWorldViewProj, mtranslate);
      matrices[0]     = LiteMath::transpose(mWVP);
    }

    if (!a_drawToShadowMap)
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_layout, 0, 1, descriptorSetWithSM + STONE_TEX, 0, NULL);
    
    vkCmdPushConstants(a_cmdBuff, a_layout, (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(float) * 3 * 16, matrices);
    m_pTeapotMesh->DrawCmd(a_cmdBuff);

    // draw bunny
    {
      auto mtranslate = LiteMath::translate4x4({ +1.25f, 0.6f, 0.5f });
      auto mscale     = LiteMath::scale4x4({ 75.0, 75.0, 75.0 });
      auto mWVP       = LiteMath::mul(a_mWorldViewProj, LiteMath::mul(mtranslate, mscale));
      matrices[0]     = LiteMath::transpose(mWVP);
    }

    if (!a_drawToShadowMap)
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_layout, 0, 1, descriptorSetWithSM + METAL_TEX, 0, NULL);
    
    vkCmdPushConstants(a_cmdBuff, a_layout, (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(float) * 3 * 16, matrices);
    m_pBunnyMesh->DrawCmd(a_cmdBuff);
  }
  

  void WriteCommandBuffer(VkRenderPass a_renderPass, VkFramebuffer a_fbo, VkImageView a_targetImageView,  VkExtent2D a_frameBufferExtent, 
                          VkPipeline a_graphicsPipeline, VkPipelineLayout a_layout,
                          VkCommandBuffer& a_cmdBuff)
  {
    vkResetCommandBuffer(a_cmdBuff, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(a_cmdBuff, &beginInfo) != VK_SUCCESS) 
      throw std::runtime_error("[WriteCommandBuffer]: failed to begin recording command buffer!");

    //// draw scene to shadow map (don't draw plane/terrain in the shadowmap)
    //
    assert(m_pShadowMap != nullptr);
    LiteMath::float4x4 lightMatrix;
    {
      m_pShadowMap->BeginRenderingToThisTexture(a_cmdBuff);

      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphicsPipelineShadow);

      const float r       = m_light.radius;
      auto mProjFix       = LiteMath::vulkanProjectionMatrixFix();
      auto mProj          = LiteMath::ortoMatrix(-r, +r, -r, +r, -m_light.lightTargetDist, m_light.lightTargetDist);
      auto mLookAt        = LiteMath::transpose(LiteMath::lookAtTransposed(m_light.cam.pos, m_light.cam.pos + m_light.cam.forward()*10.0f, m_light.cam.up));
      auto mWorldViewProj = LiteMath::mul(LiteMath::mul(mProjFix, mProj), mLookAt);

      DrawSceneCmd(a_cmdBuff, mWorldViewProj, LiteMath::float4x4(), true);

      m_pShadowMap->EndRenderingToThisTexture(a_cmdBuff);

      lightMatrix = mWorldViewProj;
      m_light.UpdatePlaneEquation();
    }

    ///// draw final scene to screen
    //
    {
      VkRenderPassBeginInfo renderPassInfo = {};
      renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass        = a_renderPass;
      renderPassInfo.framebuffer       = a_fbo;
      renderPassInfo.renderArea.offset = { 0, 0 };
      renderPassInfo.renderArea.extent = a_frameBufferExtent;

      VkClearValue clearValues[2] = {};
      clearValues[0].color           = {0.0f, 0.0f, 0.0f, 1.0f};
      clearValues[1].depthStencil    = {1.0f, 0};
      renderPassInfo.clearValueCount = 2;
      renderPassInfo.pClearValues    = &clearValues[0];

      vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline   (a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_graphicsPipeline);

      const float aspect  = float(a_frameBufferExtent.width)/float(a_frameBufferExtent.height); 
      auto mProjFix       = LiteMath::vulkanProjectionMatrixFix();  // http://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
      auto mProj          = LiteMath::transpose(LiteMath::projectionMatrixTransposed(m_cam.fov, aspect, 0.1f, 1000.0f));
      auto mLookAt        = LiteMath::transpose(LiteMath::lookAtTransposed(m_cam.pos, m_cam.pos + m_cam.forward()*10.0f, m_cam.up));
      auto mWorldViewProj = LiteMath::mul(LiteMath::mul(mProjFix, mProj), mLookAt);

      DrawSceneCmd(a_cmdBuff, mWorldViewProj, lightMatrix, false);

      vkCmdEndRenderPass(a_cmdBuff);
    }
    
    if(g_input.drawFSQuad)
    {
      float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
      m_pFSQuad->SetRenderTarget(a_targetImageView);
      m_pFSQuad->DrawCmd(a_cmdBuff, descriptorSet[SHADOW_MAP], scaleAndOffset);
    }

    if (vkEndCommandBuffer(a_cmdBuff) != VK_SUCCESS)
      throw std::runtime_error("failed to record command buffer!");  
  }


  static void CreateSyncObjects(VkDevice a_device, SyncObj* a_pSyncObjs)
  {
    a_pSyncObjs->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    a_pSyncObjs->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    a_pSyncObjs->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
    {
      if (vkCreateSemaphore(a_device, &semaphoreInfo, nullptr, &a_pSyncObjs->imageAvailableSemaphores[i]) != VK_SUCCESS ||
          vkCreateSemaphore(a_device, &semaphoreInfo, nullptr, &a_pSyncObjs->renderFinishedSemaphores[i]) != VK_SUCCESS ||
          vkCreateFence    (a_device, &fenceInfo,     nullptr, &a_pSyncObjs->inFlightFences[i]) != VK_SUCCESS) {
        throw std::runtime_error("[CreateSyncObjects]: failed to create synchronization objects for a frame!");
      }
    }
  }

  void DrawFrame() 
  {
    vkWaitForFences(device, 1, &m_sync.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences  (device, 1, &m_sync.inFlightFences[currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, screen.swapChain, UINT64_MAX, m_sync.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    
    // update next used command buffer
    //
    {
      WriteCommandBuffer(renderPass, screen.swapChainFramebuffers[imageIndex], screen.swapChainImageViews[imageIndex], screen.swapChainExtent, graphicsPipeline, pipelineLayout, 
                         commandBuffers[imageIndex]);
    }

    VkSemaphore      waitSemaphores[] = { m_sync.imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
   
    VkSubmitInfo submitInfo       = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = waitSemaphores;
    submitInfo.pWaitDstStageMask  = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[]  = { m_sync.renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_sync.inFlightFences[currentFrame]) != VK_SUCCESS)
      throw std::runtime_error("[DrawFrame]: failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;

    VkSwapchainKHR swapChains[] = { screen.swapChain };
    presentInfo.swapchainCount  = 1;
    presentInfo.pSwapchains     = swapChains;
    presentInfo.pImageIndices   = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

};

int main() 
{
  HelloTriangleApplication app;

  try 
  {
    app.run();
  }
  catch (const std::exception& e) 
  {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}