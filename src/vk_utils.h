//
// Created by frol on 15.06.19.
//

#ifndef VULKAN_MINIMAL_COMPUTE_VK_UTILS_H
#define VULKAN_MINIMAL_COMPUTE_VK_UTILS_H

#include <vulkan/vulkan.h>
#include <vector>

#include <stdexcept>
#include <sstream>
#include <cassert>

namespace vk_utils
{

  typedef VkBool32 (VKAPI_PTR *DebugReportCallbackFuncType)(VkDebugReportFlagsEXT      flags,
                                                            VkDebugReportObjectTypeEXT objectType,
                                                            uint64_t                   object,
                                                            size_t                     location,
                                                            int32_t                    messageCode,
                                                            const char*                pLayerPrefix,
                                                            const char*                pMessage,
                                                            void*                      pUserData);


  static void RunTimeError(const char* file, int line, const char* msg)
  {
    std::stringstream strout;
    strout << "runtime_error at " << file << ", line " << line << ": " << msg << std::endl;
    throw std::runtime_error(strout.str().c_str());
  }


  VkInstance CreateInstance(bool a_enableValidationLayers, std::vector<const char *>& a_enabledLayers, std::vector<const char *> a_extentions = std::vector<const char *>());
  void       InitDebugReportCallback(VkInstance a_instance, DebugReportCallbackFuncType a_callback, VkDebugReportCallbackEXT* a_debugReportCallback);
  VkPhysicalDevice FindPhysicalDevice(VkInstance a_instance, bool a_printInfo, int a_preferredDeviceId);

  uint32_t GetQueueFamilyIndex(VkPhysicalDevice a_physicalDevice, VkQueueFlagBits a_bits);
  uint32_t GetComputeQueueFamilyIndex(VkPhysicalDevice a_physicalDevice);
  VkDevice CreateLogicalDevice(uint32_t queueFamilyIndex, VkPhysicalDevice physicalDevice, const std::vector<const char *>& a_enabledLayers, std::vector<const char *> a_extentions = std::vector<const char *>());
  uint32_t FindMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice);

  //// FrameBuffer and SwapChain issues
  //
  struct ScreenBufferResources
  {
    VkSwapchainKHR             swapChain;
    std::vector<VkImage>       swapChainImages;
    VkFormat                   swapChainImageFormat;
    VkExtent2D                 swapChainExtent;
    std::vector<VkImageView>   swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
  };

  void CreateCwapChain(VkPhysicalDevice a_physDevice, VkDevice a_device, VkSurfaceKHR a_surface, int a_width, int a_height,
                       ScreenBufferResources* a_buff);

  void CreateScreenImageViews(VkDevice a_device, ScreenBufferResources* pScreen);

  void CreateScreenFrameBuffers(VkDevice a_device, VkRenderPass a_renderPass, VkImageView a_depthView, ScreenBufferResources* pScreen);

  void CreateDepthTexture(VkDevice a_device, VkPhysicalDevice a_physDevice, const int a_width, const int a_height,
                          VkDeviceMemory *a_pImageMemory, VkImage *a_image, VkImageView* a_imageView);

  std::vector<uint32_t> ReadFile(const char* filename);
  VkShaderModule CreateShaderModule(VkDevice a_device, const std::vector<uint32_t>& code);

  /**
  \brief Immediately execute command buffer and wait.
  */
  void ExecuteCommandBufferNow(VkCommandBuffer a_cmdBuff, VkQueue a_queue, VkDevice a_device);

  /**
  \brief TBD
  */
  VkCommandPool                CreateCommandPool(VkDevice a_device, VkPhysicalDevice a_physDevice, VkQueueFlagBits a_queueFlags, VkCommandPoolCreateFlagBits a_poolFlags);
  
  /**
  \brief TBD
  */
  std::vector<VkCommandBuffer> CreateCommandBuffers(VkDevice a_device, VkCommandPool a_pool, uint32_t a_buffNum);

  /**
  \brief This struction contain enough info for enabling render-to-texture in Vulkan and creating all additional Vulkan objects
  */
  struct RenderTargetInfo2D
  {
    VkExtent2D         size;           //!< image resolution
    VkFormat           fmt;            //!< image format 
    VkAttachmentLoadOp loadOp;         //!< information for renderpass
    VkImageLayout      initialLayout;  //!< information for renderpass
    VkImageLayout      finalLayout;    //!< information for renderpass
  };

  void CreateRenderPass(VkDevice a_device, RenderTargetInfo2D a_rtInfo,
                        VkRenderPass* a_pRenderPass);

  bool IsDepthFormat(VkFormat a_format);
};

#undef  RUN_TIME_ERROR
#undef  RUN_TIME_ERROR_AT
#define RUN_TIME_ERROR(e) (vk_utils::RunTimeError(__FILE__,__LINE__,(e)))
#define RUN_TIME_ERROR_AT(e, file, line) (vk_utils::RunTimeError((file),(line),(e)))

// Used for validating return values of Vulkan API calls.
//
#define VK_CHECK_RESULT(f) 													\
{																										\
    VkResult res = (f);															\
    if (res != VK_SUCCESS)													\
    {																								\
        printf("Fatal : VkResult is %d in %s at line %d\n", res,  __FILE__, __LINE__); \
        assert(res == VK_SUCCESS);									\
    }																								\
}

#endif //VULKAN_MINIMAL_COMPUTE_VK_UTILS_H
