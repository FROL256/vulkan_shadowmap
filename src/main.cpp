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
#include "vk_program.h"
#include "vk_graphics_pipeline.h"

#include "Bitmap.h"
#include "Camera.h"
#include "qmc_sobol_niederreiter.h"

const int WIDTH  = 1024;
const int HEIGHT = 1024;

const int MAX_FRAMES_IN_FLIGHT = 1;                                                    // swapchain issues
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }; // swapchain issues

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

enum SHADER_SETUP{SIMPLE_SHADOWMAP = 1, SIMPLE_PCF = 2, RAND_PCF = 3, SOBOL_PCF = 4};

/**
\brief user input with keyboard and mouse

*/
struct g_input_t
{
  bool firstMouse   = true;
  bool captureMouse = false;
  bool capturedMouseJustNow = false;
  bool drawFSQuad   = false;
  bool controlLight = false;

  float lastX, lastY, scrollY;
  float camMoveSpeed     = 1.0f;
  float mouseSensitivity = 0.1f;

  SHADER_SETUP displayedAlgorithm = SOBOL_PCF;

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

  case GLFW_KEY_LEFT_CONTROL:
    g_input.camMoveSpeed = 1.0f;
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

  case GLFW_KEY_3:
    g_input.displayedAlgorithm = SIMPLE_SHADOWMAP;
    break;

  case GLFW_KEY_4:
    g_input.displayedAlgorithm = SIMPLE_PCF;
    break;

  case GLFW_KEY_5:
    g_input.displayedAlgorithm = RAND_PCF;
    break;

  case GLFW_KEY_6:
    g_input.displayedAlgorithm = SOBOL_PCF;
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

    std::cout << std::endl;
    std::cout << "[Controls]: (3,4,5) for different shadow map methods" << std::endl;
    std::cout << "[Controls]: (9,0)   for enable/disable shadow map visualizing (red quad)" << std::endl;
    std::cout << "[Controls]: (L,C)   for control Light or Camera " << std::endl;
    std::cout << "[Controls]: (Ctrl,Shift) for different camera speed  " << std::endl;
    std::cout << "[Controls]: (left mouse button) for the GLFW focus/mouse catch  " << std::endl;
    std::cout << std::endl;

    MainLoop();

    Cleanup();
  }

private:
  
  // Vulkan basics
  //
  VkInstance               instance;
  std::vector<const char*> enabledLayers;
  VkDebugUtilsMessengerEXT debugMessenger;

  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice         device         = VK_NULL_HANDLE;
  VkQueue graphicsQueue, presentQueue, transferQueue;

  // window and swapchain specific
  //
  GLFWwindow*                     window;
  vk_utils::ScreenBufferResources screen;
  VkSurfaceKHR                    surface;

  // screen depthbuffer
  //
  VkImage        depthImage       = VK_NULL_HANDLE;
  VkDeviceMemory depthImageMemory = VK_NULL_HANDLE; 
  VkImageView    depthImageView   = VK_NULL_HANDLE;

  // main renderpass and pipelines
  //
  VkRenderPass     renderPass             = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout         = VK_NULL_HANDLE; ///!< pipeline layout that is used for both main and shadowmap pipelines
  VkPipeline       grPipelineSimple       = VK_NULL_HANDLE; ///!< simple shadowmapping
  VkPipeline       grPipelinePCF          = VK_NULL_HANDLE; ///!< shadowmapping with simple PCF
  VkPipeline       grPipelinePCFRand      = VK_NULL_HANDLE; ///!< shadowmapping with randomised PCF
  VkPipeline       graphicsPipelineShadow = VK_NULL_HANDLE; ///!< simplified pipiline for shadowmap


  // allocated memory for useful objects
  //
  VkDeviceMemory        m_memAllMeshes   = VK_NULL_HANDLE; //
  VkDeviceMemory        m_memAllTextures = VK_NULL_HANDLE;
  VkDeviceMemory        m_memShadowMap   = VK_NULL_HANDLE;

  // sync objects and command buffers per "frame-in-flight"
  //
  struct SyncObj
  {
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence>     inFlightFences;
  } m_sync;

  std::vector<VkCommandBuffer> commandBuffers;
  VkCommandPool                commandPool;

  size_t currentFrame = 0; ///<! curren frame that is displayed now! Don't render to that frame!
  Camera m_cam;

  /**
  \brief basic parameters that you need for sdhadowmappinh usually

  */
  struct ShadowMapCam
  {
    ShadowMapCam() 
    {  
      cam.pos    = float3(10.0f, 10.0f, 10.0f);
      cam.lookAt = float3(0, 0, 0);
      cam.up     = float3(0, 1, 0);
  
      radius          = 5.0f;
      lightTargetDist = 20.0f;
      usePerspectiveM = false;
    }

    float  radius;           ///!< ignored when usePerspectiveM == true 
    float  lightTargetDist;  ///!< identify depth range
    Camera cam;              ///!< user control for light to later get light worldViewProj matrix
    bool   usePerspectiveM;  ///!< use perspective matrix if true and ortographics otherwise
  
  } m_light;
  
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  /**
  \brief Simple copy engine implementation. Our mesh and texture helpers require use to implement copy by himself.
         So, we make such implementation by calling simple helpers from vk_copy::SimpleCopyHelper

  */
  struct CopyEngine : public vk_geom::ICopyEngine, 
                      public vk_texture::ICopyEngine
  {
    CopyEngine(VkPhysicalDevice a_physicalDevice, VkDevice a_device, VkQueue a_transferQueue, size_t a_stagingBuffSize) : m_helper(a_physicalDevice, a_device, a_transferQueue, a_stagingBuffSize) {}
  
    void UpdateBuffer(VkBuffer a_dst, size_t a_dstOffset, const void* a_src, size_t a_size)    override { m_helper.UpdateBuffer(a_dst, a_dstOffset, a_src, a_size); }
    void UpdateImage(VkImage a_image, const void* a_src, int a_width, int a_height, int a_bpp) override { m_helper.UpdateImage(a_image, a_src, a_width, a_height, a_bpp); }

    VkCommandBuffer CmdBuffer() { return m_helper.CmdBuffer(); }

    vk_copy::SimpleCopyHelper m_helper;
  };

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<CopyEngine>       m_pCopyHelper;
  std::shared_ptr<vk_geom::IMesh>   m_pTerrainMesh;
  std::shared_ptr<vk_geom::IMesh>   m_pTeapotMesh;
  std::shared_ptr<vk_geom::IMesh>   m_pBunnyMesh;
  std::shared_ptr<vk_utils::FSQuad> m_pFSQuad;

  enum {TERRAIN_TEX = 0, STONE_TEX = 1, METAL_TEX = 2, AUX_TEX_ROT = 3, TEXTURES_NUM = 4 };  

  std::shared_ptr<vk_texture::SimpleTexture2D>     m_pTex[TEXTURES_NUM];
  std::shared_ptr<vk_texture::RenderableTexture2D> m_pShadowMap;

  static constexpr int TEX_ROT_WIDTH  = 256;
  static constexpr int TEX_ROT_HEIGHT = 256;
  static constexpr int TEX_SAMPLES_PP = 8;

  // Descriptors represent resources in shaders. They allow us to use things like
  // uniform buffers, storage buffers and images in GLSL.
  // A single descriptor represents a single resource, and several descriptors are organized
  // into descriptor sets, which are basically just collections of descriptors.
  // 
  VkDescriptorSet       descriptorSetForQuad; // for our textures
  VkDescriptorSet       descriptorSetWithSM[TEXTURES_NUM] = {};
  VkDescriptorSetLayout descriptorSetLayoutQuad = nullptr;
  VkDescriptorSetLayout descriptorSetLayoutSM   = nullptr;

  std::shared_ptr<vk_utils::ProgramBindings> m_pBindings = nullptr;

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

    // logical device, queues and command pool
    //
    device = vk_utils::CreateLogicalDevice(queueFID, physicalDevice, enabledLayers, deviceExtensions);
    vkGetDeviceQueue(device, queueFID,     0, &graphicsQueue);
    vkGetDeviceQueue(device, queueFID,     0, &presentQueue);
    vkGetDeviceQueue(device, queueCopyFID, 0, &transferQueue);

    // helper copy engine (for copying from CPU to GPU)
    //
    m_pCopyHelper = std::make_unique<CopyEngine>(physicalDevice, device, transferQueue, 64*1024*1024);

    // helper object that simplify descriptor sets creation
    //
    VkDescriptorType dtypes[2] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
    m_pBindings = std::make_shared<vk_utils::ProgramBindings>(device, dtypes, 2, 64);

    CreateSyncObjects(device, &m_sync);
  }

  void CreateResources()
  {
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // swap chain and screen images
    //
    vk_utils::CreateCwapChain(physicalDevice, device, surface, WIDTH, HEIGHT,
                              &screen);

    vk_utils::CreateScreenImageViews(device, &screen);

    vk_utils::CreateDepthTexture(device, physicalDevice, WIDTH, HEIGHT, 
                                 &depthImageMemory, &depthImage, &depthImageView);

    CreateRenderPass(device, screen.swapChainImageFormat, 
                     &renderPass);
  
    CreateScreenFrameBuffers(device, renderPass, depthImageView, 
                             &screen);

    // create basic command buffers
    //
    commandPool    = vk_utils::CreateCommandPool(device, physicalDevice, VK_QUEUE_GRAPHICS_BIT, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandBuffers = vk_utils::CreateCommandBuffers(device, commandPool, screen.swapChainFramebuffers.size());

    m_pFSQuad = std::make_shared<vk_utils::FSQuad>();
    m_pFSQuad->Create(device, "shaders/quad_vert.spv", "shaders/quad_frag.spv", 
                      vk_utils::RenderTargetInfo2D{ VkExtent2D{ WIDTH, HEIGHT }, screen.swapChainImageFormat, 
                                                    VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR });
 

    std::cout << "[CreateResources]: loading textures ... " << std::endl;

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
    auto memReqTex5 = m_pTex[AUX_TEX_ROT]->CreateImage(device, TEX_ROT_WIDTH, TEX_ROT_HEIGHT, VK_FORMAT_R32G32B32A32_UINT);
    auto memReqTex4 = m_pShadowMap->CreateImage(device, 2048, 2048, VK_FORMAT_D16_UNORM);

    assert(memReqTex1.memoryTypeBits == memReqTex2.memoryTypeBits);
    assert(memReqTex1.memoryTypeBits == memReqTex3.memoryTypeBits);
    assert(memReqTex1.memoryTypeBits == memReqTex5.memoryTypeBits);
    //assert(memReqTex1.memoryTypeBits == memReqTex4.memoryTypeBits); // well, usually not, please make separate allocation for shadow map :)
    
    // memory for all read-only textures
    {
      VkMemoryAllocateInfo allocateInfo = {};
      allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocateInfo.pNext           = nullptr;
      allocateInfo.allocationSize  = memReqTex1.size + memReqTex2.size + memReqTex3.size + memReqTex5.size;
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

    m_pTex[TERRAIN_TEX]->BindMemory(m_memAllTextures, 0);
    m_pTex[STONE_TEX]->BindMemory  (m_memAllTextures, memReqTex1.size);
    m_pTex[METAL_TEX]->BindMemory  (m_memAllTextures, memReqTex1.size + memReqTex2.size);
    m_pTex[AUX_TEX_ROT]->BindMemory(m_memAllTextures, memReqTex1.size + memReqTex2.size + memReqTex3.size);
    m_pShadowMap->BindMemory       (m_memShadowMap, 0);


    // DS for drawing objects
    //
    m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pBindings->BindImage(0, m_pTex[TERRAIN_TEX]->View(), m_pTex[TERRAIN_TEX]->Sampler());
    m_pBindings->BindImage(1, m_pShadowMap->View(), m_pShadowMap->Sampler());
    m_pBindings->BindImage(2, m_pTex[AUX_TEX_ROT]->View(), m_pTex[AUX_TEX_ROT]->Sampler());
    m_pBindings->BindEnd(&descriptorSetWithSM[TERRAIN_TEX], &descriptorSetLayoutSM);

    m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pBindings->BindImage(0, m_pTex[STONE_TEX]->View(), m_pTex[STONE_TEX]->Sampler());
    m_pBindings->BindImage(1, m_pShadowMap->View(), m_pShadowMap->Sampler());
    m_pBindings->BindImage(2, m_pTex[AUX_TEX_ROT]->View(), m_pTex[AUX_TEX_ROT]->Sampler());
    m_pBindings->BindEnd(&descriptorSetWithSM[STONE_TEX]);

    m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pBindings->BindImage(0, m_pTex[METAL_TEX]->View(), m_pTex[METAL_TEX]->Sampler());
    m_pBindings->BindImage(1, m_pShadowMap->View(), m_pShadowMap->Sampler());
    m_pBindings->BindImage(2, m_pTex[AUX_TEX_ROT]->View(), m_pTex[AUX_TEX_ROT]->Sampler());
    m_pBindings->BindEnd(&descriptorSetWithSM[METAL_TEX]);

    // DS for srawing quad
    //
    m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pBindings->BindImage(0, m_pShadowMap->View(), m_pShadowMap->Sampler());
    m_pBindings->BindEnd(&descriptorSetForQuad, &descriptorSetLayoutQuad);
    
    m_pTex[TERRAIN_TEX]->Update(data1.data(), w1, h1, sizeof(int), m_pCopyHelper.get()); // --> put m_pTex[i] in transfer_dst layout
    m_pTex[STONE_TEX]->Update(data2.data(), w2, h2, sizeof(int), m_pCopyHelper.get());   // --> put m_pTex[i] in transfer_dst layout
    m_pTex[METAL_TEX]->Update(data3.data(), w3, h3, sizeof(int), m_pCopyHelper.get());   // --> put m_pTex[i] in transfer_dst layout
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[CreateResources]: generating random numbers ... " << std::endl;

    auto samplesF          = MakeSortedByPixel_QRND_2D_DISK(TEX_ROT_WIDTH, TEX_ROT_HEIGHT, TEX_SAMPLES_PP);
    //for (size_t i = 0; i < samplesF.size(); i++)
      //samplesF[i] = rnd_simple(-1.0f, 1.0f);

    auto samplesCompressed = Compress8SamplesToUint32x4(samplesF, TEX_ROT_WIDTH, TEX_ROT_HEIGHT);

    m_pTex[AUX_TEX_ROT]->Update(samplesCompressed.data(), TEX_ROT_WIDTH, TEX_ROT_HEIGHT, sizeof(uint32_t)*4, m_pCopyHelper.get());
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // generate all mips
    //
    {
      VkCommandBuffer cmdBuff = m_pCopyHelper->CmdBuffer(); // you can any other command buffer with transfer capability here ... 

      vkResetCommandBuffer(cmdBuff, 0);
      VkCommandBufferBeginInfo beginInfo = {};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
      if (vkBeginCommandBuffer(cmdBuff, &beginInfo) != VK_SUCCESS) 
         throw std::runtime_error("[FFF]: failed to begin command buffer!");
      
      for (int i = 0; i<TEXTURES_NUM; i++) // don't generate mips for auxilarry texture
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

    std::cout << "[CreateResources]: creating shaders/pipelines ... " << std::endl;

    vk_utils::GraphicsPipelineCreateInfo grmaker;
    
    pipelineLayout = grmaker.Layout_Simple3D_VSFS(device, WIDTH, HEIGHT, descriptorSetLayoutSM, 4 * 16 * sizeof(float));

    grmaker.Shaders_VSFS(device, "shaders/cmesh_t3v4x2.spv", "shaders/direct_light_simple.spv");
    grPipelineSimple = grmaker.Pipeline(device, m_pTerrainMesh->VertexInputLayout(), renderPass);

    grmaker.Shaders_VSFS(device, "shaders/cmesh_t3v4x2.spv", "shaders/direct_light_pcf.spv");
    grPipelinePCF = grmaker.Pipeline(device, m_pTerrainMesh->VertexInputLayout(), renderPass);

    grmaker.Shaders_VSFS(device, "shaders/cmesh_t3v4x2.spv", "shaders/direct_light_pcf_rand.spv");
    grPipelinePCFRand = grmaker.Pipeline(device, m_pTerrainMesh->VertexInputLayout(), renderPass);

    grmaker.Shaders_VS(device, "shaders/cmesh_t3v4x2.spv");
    grmaker.viewport.width  = +(float)m_pShadowMap->Width();
    grmaker.viewport.height = +(float)m_pShadowMap->Height();
    grmaker.scissor.extent  = VkExtent2D{ uint32_t(m_pShadowMap->Width()), uint32_t(m_pShadowMap->Height()) };

    graphicsPipelineShadow  = grmaker.Pipeline(device, m_pTerrainMesh->VertexInputLayout(), m_pShadowMap->Renderpass());
 
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
    m_pTeapotMesh  = nullptr; // smart pointer will destroy resources
    m_pBunnyMesh   = nullptr; // smart pointer will destroy resources
    m_pFSQuad      = nullptr; // smart pointer will destroy resources
    m_pBindings    = nullptr; // smart pointer will destroy resources

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

    vkDestroyPipeline(device, grPipelineSimple, nullptr);
    if(graphicsPipelineShadow != nullptr)
      vkDestroyPipeline(device, graphicsPipelineShadow, nullptr);
    if(grPipelinePCF != VK_NULL_HANDLE)
      vkDestroyPipeline(device, grPipelinePCF, nullptr);
    if (grPipelinePCFRand != VK_NULL_HANDLE)
      vkDestroyPipeline(device, grPipelinePCFRand, nullptr);

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

  /**
  \brief this function draw (i.e. `drawing commands in the command buffer) scene once 
  \param a_cmdBuff         - output command buffer in wich commands will be written to
  \param a_mWorldViewProj  - input  world view projection matrix; assume Vulkan coordinate system fpr Proj.
  \param a_lightMatrix     - input  light matrix (transform world space to tangent space); ignored if a_drawToShadowMap == false;
  \param a_drawToShadowMap - input flag that signals we drawing geometry only in the shadow map
  */
  void DrawSceneCmd(VkCommandBuffer a_cmdBuff, float4x4 a_mWorldViewProj, float4x4 a_lightMatrix, bool a_drawToShadowMap)
  {
    float3 a_lightDir = normalize(m_light.cam.pos - m_light.cam.lookAt);
    auto   a_layout   = pipelineLayout;

    // draw plane/terrain
    //
    float4x4 matrices[4];

    {
      matrices[3].set_col(0, to_float4(m_cam.pos, 0.0f));                     // put wCamPos
      matrices[3].set_col(1, to_float4(a_lightDir, m_light.lightTargetDist)); // put lightDir
      matrices[3].set_col(2, float4(TEX_ROT_WIDTH, TEX_ROT_HEIGHT, 1.5f / m_pShadowMap->Width(), 0.0f));
    }

    {
      auto mrot   = cmath::rotate4x4X(-cmath::DEG_TO_RAD*90.0f);
      auto mWVP   = a_mWorldViewProj*mrot;
      auto mWVPL  = a_lightMatrix*mrot;
      matrices[0] = mWVP;
      matrices[1] = mWVPL;
      matrices[2] = mrot;
    }

    if (!a_drawToShadowMap) // in this particular sample we don't want to draw ground in the shadow map
    {
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_layout, 0, 1, descriptorSetWithSM + TERRAIN_TEX, 0, NULL);
      vkCmdPushConstants(a_cmdBuff, a_layout, (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(float) * 4 * 16, matrices);
      m_pTerrainMesh->DrawCmd(a_cmdBuff);
    }

    // draw teapot
    {
      auto mtranslate = cmath::translate4x4({ -0.5f, 0.4f, -0.5f });
      auto mWVP       = a_mWorldViewProj*mtranslate;
      auto mWVPL      = a_lightMatrix*mtranslate;
      matrices[0]     = mWVP;
      matrices[1]     = mWVPL;
      matrices[2]     = cmath::float4x4();
    }
   
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_layout, 0, 1, descriptorSetWithSM + STONE_TEX, 0, NULL);
    vkCmdPushConstants(a_cmdBuff, a_layout, (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(float) * 4 * 16, matrices);
    m_pTeapotMesh->DrawCmd(a_cmdBuff);

    // draw bunny
    {
      auto mtranslate = cmath::translate4x4({ +1.25f, 0.6f, 0.5f });
      auto mscale     = cmath::scale4x4({ 75.0, 75.0, 75.0 });
      auto mWVP       = a_mWorldViewProj*mtranslate*mscale;
      auto mWVPL      = a_lightMatrix*mtranslate*mscale;
      matrices[0]     = mWVP;
      matrices[1]     = mWVPL;
      matrices[2]     = cmath::float4x4();
    }

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_layout, 0, 1, descriptorSetWithSM + METAL_TEX, 0, NULL);
    vkCmdPushConstants(a_cmdBuff, a_layout, (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(float) * 4 * 16, matrices);
    m_pBunnyMesh->DrawCmd(a_cmdBuff);
  }
  
  /**
  \brief Main draw function
  \param a_cmdBuff - output command buffer in wich commands will be written to 
  \param a_fbo     - output framebuffer where we are going to render
  \param a_targetImageView - auxilary image view that is related to a_fbo; needed for drawing debug quad only

  */
  void DrawFrameCmd(VkCommandBuffer& a_cmdBuff, VkFramebuffer a_fbo, VkImageView a_targetImageView)
  {
    VkExtent2D a_frameBufferExtent = this->screen.swapChainExtent;
    
    VkPipeline a_graphicsPipeline = VK_NULL_HANDLE; 
    if (g_input.displayedAlgorithm == SIMPLE_SHADOWMAP)
      a_graphicsPipeline = this->grPipelineSimple;
    else if (g_input.displayedAlgorithm == SIMPLE_PCF)
      a_graphicsPipeline = this->grPipelinePCF;
    else
      a_graphicsPipeline = this->grPipelinePCFRand;

    VkPipelineLayout a_layout      = this->pipelineLayout;
    VkRenderPass     a_renderPass  = this->renderPass;

    vkResetCommandBuffer(a_cmdBuff, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(a_cmdBuff, &beginInfo) != VK_SUCCESS) 
      throw std::runtime_error("[WriteCommandBuffer]: failed to begin recording command buffer!");

    //// draw scene to shadow map (don't draw plane/terrain in the shadowmap)
    //
    assert(m_pShadowMap != nullptr);
    float4x4 lightMatrix;
    {
      m_pShadowMap->BeginRenderingToThisTexture(a_cmdBuff);

      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphicsPipelineShadow);

      float4x4 mProj;
      if(m_light.usePerspectiveM)
        mProj = cmath::perspectiveMatrix(m_light.cam.fov, 1.0f, 1.0f, m_light.lightTargetDist*2.0f);
      else
        mProj = cmath::ortoMatrix(-m_light.radius, +m_light.radius, -m_light.radius, +m_light.radius, 0.0f, m_light.lightTargetDist);
        
      auto mProjFix       = m_light.usePerspectiveM ? cmath::float4x4() : cmath::OpenglToVulkanProjectionMatrixFix(); // don't understang why fix is not needed for perspective case for shadowmap ... it works for common rendering
      auto mLookAt        = cmath::lookAt(m_light.cam.pos, m_light.cam.pos + m_light.cam.forward()*10.0f, m_light.cam.up);
      auto mWorldViewProj = mProjFix*mProj*mLookAt;

      DrawSceneCmd(a_cmdBuff, mWorldViewProj, cmath::float4x4(), true);

      m_pShadowMap->EndRenderingToThisTexture(a_cmdBuff);

      lightMatrix = mWorldViewProj;
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
      auto mProjFix       = cmath::OpenglToVulkanProjectionMatrixFix();  // http://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
      auto mProj          = cmath::perspectiveMatrix(m_cam.fov, aspect, 0.1f, 1000.0f);
      auto mLookAt        = cmath::lookAt(m_cam.pos, m_cam.pos + m_cam.forward()*10.0f, m_cam.up);
      auto mWorldViewProj = mProjFix*mProj*mLookAt;

      DrawSceneCmd(a_cmdBuff, mWorldViewProj, lightMatrix, false);

      vkCmdEndRenderPass(a_cmdBuff);
    }
    
    if(g_input.drawFSQuad)
    {
      float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
      m_pFSQuad->SetRenderTarget(a_targetImageView);
      m_pFSQuad->DrawCmd(a_cmdBuff, descriptorSetForQuad, scaleAndOffset);
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

  /**
  \brief This function is a general example of async frame rendering and displaying in Vulkan.
         It rotates several objects for parallel rendering abd displaying image.
      
         Further objects are rotated: (commandBuffers, screen.swapChainFramebuffers, screen.swapChainImageViews, m_sync.inFlightFences)
  */
  void DrawFrame() 
  {
    vkWaitForFences(device, 1, &m_sync.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences  (device, 1, &m_sync.inFlightFences[currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, screen.swapChain, UINT64_MAX, m_sync.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    
    // update next used command buffer
    //
    DrawFrameCmd(commandBuffers[imageIndex], screen.swapChainFramebuffers[imageIndex], screen.swapChainImageViews[imageIndex]);

    VkSemaphore      waitSemaphores[] = { m_sync.imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
   
    VkSubmitInfo submitInfo       = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = waitSemaphores;
    submitInfo.pWaitDstStageMask  = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffers[imageIndex]; // note that we will draw from this command buffer now

    VkSemaphore signalSemaphores[]  = { m_sync.renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_sync.inFlightFences[currentFrame]) != VK_SUCCESS)
      throw std::runtime_error("[DrawFrame]: failed to submit draw command buffer!");

    // as well as submitting drawing, we should submit present
    //
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

  HelloTriangleApplication app; // Everybody does in this way due to Vulkan reauire a lot of objects to manage.
                                // Putting all of them in global variables assumed as bad design, but IMHO for such sample there is no difference ...
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