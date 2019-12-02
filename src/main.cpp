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

  VkRenderPass     renderPass;
  VkPipelineLayout pipelineLayout;
  VkPipeline       graphicsPipeline;

  VkCommandPool                commandPool;
  std::vector<VkCommandBuffer> commandBuffers;

  // Descriptors represent resources in shaders. They allow us to use things like
  // uniform buffers, storage buffers and images in GLSL.
  // A single descriptor represents a single resource, and several descriptors are organized
  // into descriptor sets, which are basically just collections of descriptors.
  // 
  VkDescriptorPool      descriptorPool      = nullptr;
  VkDescriptorSet       descriptorSet[3]    = {}; // for our textures
  VkDescriptorSetLayout descriptorSetLayout = nullptr;

  VkDeviceMemory        m_memAllMeshes   = nullptr; //
  VkDeviceMemory        m_memAllTextures = nullptr;

  struct SyncObj
  {
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence>     inFlightFences;
  } m_sync;

  size_t currentFrame = 0;
  Camera m_cam;
  
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


  std::unique_ptr<CopyEngine>     m_pCopyHelper;
  std::shared_ptr<vk_geom::IMesh> m_pTerrainMesh;
  std::shared_ptr<vk_geom::IMesh> m_pTeapotMesh;
  std::shared_ptr<vk_geom::IMesh> m_pLucyMesh;

  std::shared_ptr<vk_texture::Texture2D> m_pTex1, m_pTex2, m_pTex3;


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

  
    CreateSyncObjects(device, &m_sync);

    CreateDescriptorSetLayout(device, &descriptorSetLayout);

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


    m_pTex1    = std::make_shared<vk_texture::Texture2D>();
    m_pTex2    = std::make_shared<vk_texture::Texture2D>();
    m_pTex3    = std::make_shared<vk_texture::Texture2D>();
    
    auto memReqTex1 = m_pTex1->CreateImage(device, w1, h1, VK_FORMAT_R8G8B8A8_UNORM);
    auto memReqTex2 = m_pTex2->CreateImage(device, w2, h2, VK_FORMAT_R8G8B8A8_UNORM);
    auto memReqTex3 = m_pTex3->CreateImage(device, w3, h3, VK_FORMAT_R8G8B8A8_UNORM);

    assert(memReqTex1.memoryTypeBits == memReqTex2.memoryTypeBits);
    assert(memReqTex1.memoryTypeBits == memReqTex3.memoryTypeBits);

    {
      VkMemoryAllocateInfo allocateInfo = {};
      allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocateInfo.pNext           = nullptr;
      allocateInfo.allocationSize  = memReqTex1.size + memReqTex2.size + memReqTex3.size;
      allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(memReqTex1.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physicalDevice);

      VK_CHECK_RESULT(vkAllocateMemory(device, &allocateInfo, NULL, &m_memAllTextures));
    }

    m_pTex1->BindMemory(m_memAllTextures, 0);
    m_pTex2->BindMemory(m_memAllTextures, memReqTex1.size);
    m_pTex3->BindMemory(m_memAllTextures, memReqTex1.size + memReqTex2.size);
    
    VkSampler samplers[] = { m_pTex1->Sampler(), m_pTex2->Sampler(), m_pTex3->Sampler()};
    VkImageView views [] = { m_pTex1->View(),    m_pTex2->View(), m_pTex3->View()      };

    CreateDescriptorSetsForImages(device, descriptorSetLayout, samplers, views, 3,
                                  &descriptorPool, descriptorSet);

    m_pTex1->Update(data1.data(), w1, h1, sizeof(int), m_pCopyHelper.get()); // --> put m_pTex1 in transfer_dst
    m_pTex2->Update(data2.data(), w2, h2, sizeof(int), m_pCopyHelper.get()); // --> put m_pTex2 in transfer_dst
    m_pTex3->Update(data3.data(), w3, h3, sizeof(int), m_pCopyHelper.get()); // --> put m_pTex3 in transfer_dst
    
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
      
      m_pTex1->GenerateMipsCmd(cmdBuff, transferQueue);                             // --> put m_pTex1 in shader read optimal
      m_pTex2->GenerateMipsCmd(cmdBuff, transferQueue);                             // --> put m_pTex2 in shader read optimal
      m_pTex3->GenerateMipsCmd(cmdBuff, transferQueue);                             // --> put m_pTex3 in shader read optimal
     
      vkEndCommandBuffer(cmdBuff);

      vk_utils::ExecuteCommandBufferNow(cmdBuff, transferQueue, device);
    }

    // create meshes
    //
    m_pTerrainMesh = std::make_shared< vk_geom::CompactMesh_T3V4x2F >();
    m_pTeapotMesh  = std::make_shared< vk_geom::CompactMesh_T3V4x2F >();
    m_pLucyMesh    = std::make_shared< vk_geom::CompactMesh_T3V4x2F >();

    auto meshData = cmesh::CreateQuad(64, 64, 4.0f);
    auto teapData = cmesh::LoadMeshFromVSGF("data/teapot.vsgf");
    auto lucyData = cmesh::LoadMeshFromVSGF("data/bunny0.vsgf");

    if(teapData.VerticesNum() == 0)
      RUN_TIME_ERROR("can't load mesh at 'data/teapot.vsgf'");

    auto memReq1 = m_pTerrainMesh->CreateBuffers(device, int(meshData.VerticesNum()), int(meshData.IndicesNum())); // what if memReq1 and memReq2 differs in memoryTypeBits ... ? )
    auto memReq2 = m_pTeapotMesh->CreateBuffers (device, int(teapData.VerticesNum()), int(teapData.IndicesNum())); //
    auto memReq3 = m_pLucyMesh->CreateBuffers   (device, int(lucyData.VerticesNum()), int(lucyData.IndicesNum())); //

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
    m_pLucyMesh->BindBuffers   (m_memAllMeshes, memReq1.size + memReq2.size);

    m_pTerrainMesh->UpdateBuffers(meshData, m_pCopyHelper.get());
    m_pTeapotMesh->UpdateBuffers (teapData, m_pCopyHelper.get());
    m_pLucyMesh->UpdateBuffers   (lucyData, m_pCopyHelper.get()); 

    CreateGraphicsPipeline(device, screen.swapChainExtent, renderPass, 
                           &pipelineLayout, &graphicsPipeline);

    CreateAndWriteCommandBuffers(device, commandPool, screen.swapChainFramebuffers, screen.swapChainExtent, renderPass, graphicsPipeline, pipelineLayout,
                                 &commandBuffers);

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

    m_pTex1 = nullptr;        // smart pointer will destroy resources
    m_pTex2 = nullptr;
    m_pTex3 = nullptr;

    m_pCopyHelper  = nullptr; // smart pointer will destroy resources
    m_pTerrainMesh = nullptr; // smart pointer will destroy resources
    m_pTeapotMesh  = nullptr;
    m_pLucyMesh    = nullptr;

    // free our vbos
    vkFreeMemory(device, m_memAllMeshes, NULL);

    if(m_memAllTextures != nullptr)
      vkFreeMemory(device, m_memAllTextures, NULL);

    if(depthImageMemory != nullptr)
    {
      vkFreeMemory      (device, depthImageMemory, NULL);
      vkDestroyImageView(device, depthImageView, NULL);
      vkDestroyImage    (device, depthImage, NULL);
    }

    if (descriptorPool != nullptr)
      vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    if(descriptorSetLayout != nullptr)
      vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);

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

    vkDestroyPipeline      (device, graphicsPipeline, nullptr);
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

  static void CreateDescriptorSetLayout(VkDevice a_device, VkDescriptorSetLayout *a_pDSLayout)
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

  void CreateDescriptorSetsForImages(VkDevice a_device, const VkDescriptorSetLayout a_dsLayout, VkSampler* a_samplers, VkImageView *a_views, int a_imagesNumber,
                                     VkDescriptorPool* a_pDSPool, VkDescriptorSet* a_pDS)
  {
    assert(a_pDSPool   != nullptr);
    assert(a_pDS       != nullptr);

    assert(a_samplers != nullptr);
    assert(a_views    != nullptr);
    assert(a_imagesNumber != 0);

    std::vector<VkDescriptorPoolSize> descriptorPoolSize(a_imagesNumber);
    for(int i=0;i<a_imagesNumber;i++)
    {
      descriptorPoolSize[i].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorPoolSize[i].descriptorCount = 1;
    }
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets       = a_imagesNumber; // we need to allocate at least 1 descriptor set
    descriptorPoolCreateInfo.poolSizeCount = a_imagesNumber;
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

  void CreateGraphicsPipeline(VkDevice a_device, VkExtent2D a_screenExtent, VkRenderPass a_renderPass,
                              VkPipelineLayout* a_pLayout, VkPipeline* a_pPipiline)
  {
    auto vertShaderCode = vk_utils::ReadFile("shaders/cmesh_t3v4x2.spv");
    auto fragShaderCode = vk_utils::ReadFile("shaders/direct_light.spv");

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
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float)a_screenExtent.width;
    viewport.height   = (float)a_screenExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

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
    pcRange.size       = 16*2*sizeof(float) + 4*sizeof(float);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pcRange;
    pipelineLayoutInfo.pSetLayouts            = &descriptorSetLayout;
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

    assert(m_pTerrainMesh != nullptr);
    auto vertexInputInfo = m_pTerrainMesh->VertexInputLayout();

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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

    if (vkCreateGraphicsPipelines(a_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, a_pPipiline) != VK_SUCCESS)
      throw std::runtime_error("[CreateGraphicsPipeline]: failed to create graphics pipeline!");

    vkDestroyShaderModule(a_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(a_device, vertShaderModule, nullptr);
  }

  void WriteCommandBuffer(VkRenderPass a_renderPass, VkFramebuffer a_fbo,  VkExtent2D a_frameBufferExtent, 
                          VkPipeline a_graphicsPipeline, VkPipelineLayout a_layout,
                          VkCommandBuffer& a_cmdBuff)
  {
    vkResetCommandBuffer(a_cmdBuff, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(a_cmdBuff, &beginInfo) != VK_SUCCESS) 
      throw std::runtime_error("[WriteCommandBuffer]: failed to begin recording command buffer!");

    ///// 
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

      vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_graphicsPipeline);
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_layout, 0, 1, descriptorSet+0, 0, NULL);

      const float aspect   = float(a_frameBufferExtent.width)/float(a_frameBufferExtent.height); 
      auto mProjTransposed = LiteMath::projectionMatrixTransposed(m_cam.fov, aspect, 0.1f, 1000.0f);
      auto mLookAt         = LiteMath::transpose(LiteMath::lookAtTransposed(m_cam.pos, m_cam.pos + m_cam.forward()*10.0f, m_cam.up));

      // draw plane
      LiteMath::float4x4 matrices[3];
      {
        auto mrot   = LiteMath::rotate_X_4x4(LiteMath::DEG_TO_RAD*90.0f);
        matrices[0] = LiteMath::transpose(LiteMath::mul(mLookAt, mrot));  
        matrices[1] = mProjTransposed;
        matrices[2].row[0].x = m_cam.pos.x;  matrices[2].row[0].y = m_cam.pos.y;  matrices[2].row[0].z = m_cam.pos.z; // pass cam pos to shader
      }
      vkCmdPushConstants(a_cmdBuff, a_layout, (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(float)*2*16 + 4*sizeof(float), matrices);
      m_pTerrainMesh->DrawCmd(a_cmdBuff);
      
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_layout, 0, 1, descriptorSet+1, 0, NULL);

      // draw teapot
      {
        auto mtranslate = LiteMath::translate4x4({-0.5f, 0.4f, -0.5f});
        matrices[0]     = LiteMath::transpose(LiteMath::mul(mLookAt, mtranslate));  
        matrices[1]     = mProjTransposed;
        matrices[2].row[0].x = m_cam.pos.x;  matrices[2].row[0].y = m_cam.pos.y;  matrices[2].row[0].z = m_cam.pos.z; // pass cam pos to shader
      }
      vkCmdPushConstants(a_cmdBuff, a_layout, (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(float)*2*16 + 4*sizeof(float), matrices);
      m_pTeapotMesh->DrawCmd(a_cmdBuff);
      
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_layout, 0, 1, descriptorSet+2, 0, NULL);

      // draw bunny
      {
        auto mtranslate = LiteMath::translate4x4({+1.25f, 0.6f, 0.5f});
        auto mscale     = LiteMath::scale4x4({75.0, 75.0, 75.0});
        matrices[0]     = LiteMath::transpose(LiteMath::mul(mLookAt, (LiteMath::mul(mtranslate, mscale))));  
        matrices[1]     = mProjTransposed;
        matrices[2].row[0].x = m_cam.pos.x;  matrices[2].row[0].y = m_cam.pos.y;  matrices[2].row[0].z = m_cam.pos.z; // pass cam pos to shader
      }

      vkCmdPushConstants(a_cmdBuff, a_layout, (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(float)*2*16 + 4*sizeof(float), matrices);
      m_pLucyMesh->DrawCmd(a_cmdBuff);

      vkCmdEndRenderPass(a_cmdBuff);
    }
  
    if (vkEndCommandBuffer(a_cmdBuff) != VK_SUCCESS)
      throw std::runtime_error("failed to record command buffer!");  
  }


  void CreateAndWriteCommandBuffers(VkDevice a_device, VkCommandPool a_cmdPool, std::vector<VkFramebuffer> a_swapChainFramebuffers, VkExtent2D a_frameBufferExtent,
                                    VkRenderPass a_renderPass, VkPipeline a_graphicsPipeline, VkPipelineLayout a_layout,
                                    std::vector<VkCommandBuffer>* a_cmdBuffers) 
  {
    std::vector<VkCommandBuffer>& commandBuffers = (*a_cmdBuffers);

    commandBuffers.resize(a_swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = a_cmdPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(a_device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
      throw std::runtime_error("[CreateCommandPoolAndBuffers]: failed to allocate command buffers!");

    for (size_t i = 0; i < commandBuffers.size(); i++) 
    {
      WriteCommandBuffer(a_renderPass, a_swapChainFramebuffers[i], a_frameBufferExtent, a_graphicsPipeline, a_layout, 
                         commandBuffers[i]);
    }
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
      WriteCommandBuffer(renderPass, screen.swapChainFramebuffers[imageIndex], screen.swapChainExtent, graphicsPipeline, pipelineLayout, 
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