//
// Created by frol on 15.06.19.
//

#include "vk_utils.h"

#include <string.h>
#include <assert.h>
#include <iostream>

#include <cmath>
#include <cassert>

#include <algorithm>
#ifdef WIN32
#undef min
#undef max
#endif 

static const char* g_validationLayerData = "VK_LAYER_LUNARG_standard_validation";
static const char* g_debugReportExtName  = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


VkInstance vk_utils::CreateInstance(bool a_enableValidationLayers, std::vector<const char *>& a_enabledLayers, std::vector<const char *> a_extentions)
{
  std::vector<const char *> enabledExtensions = a_extentions;

  /*
  By enabling validation layers, Vulkan will emit warnings if the API
  is used incorrectly. We shall enable the layer VK_LAYER_LUNARG_standard_validation,
  which is basically a collection of several useful validation layers.
  */
  if (a_enableValidationLayers)
  {
    /*
    We get all supported layers with vkEnumerateInstanceLayerProperties.
    */
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);

    std::vector<VkLayerProperties> layerProperties(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

    /*
    And then we simply check if VK_LAYER_LUNARG_standard_validation is among the supported layers.
    */
    bool foundLayer = false;
    for (VkLayerProperties prop : layerProperties) {

      if (strcmp("VK_LAYER_LUNARG_standard_validation", prop.layerName) == 0) {
        foundLayer = true;
        break;
      }

    }

    if (!foundLayer)
      RUN_TIME_ERROR("Layer VK_LAYER_LUNARG_standard_validation not supported\n");

    a_enabledLayers.push_back(g_validationLayerData); // Alright, we can use this layer.

    /*
    We need to enable an extension named VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    in order to be able to print the warnings emitted by the validation layer.

    So again, we just check if the extension is among the supported extensions.
    */
    uint32_t extensionCount;

    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensionProperties.data());

    bool foundExtension = false;
    for (VkExtensionProperties prop : extensionProperties) {
      if (strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, prop.extensionName) == 0) {
        foundExtension = true;
        break;
      }

    }

    if (!foundExtension)
      RUN_TIME_ERROR("Extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME not supported\n");

    enabledExtensions.push_back(g_debugReportExtName);
  }

  /*
  Next, we actually create the instance.
  */

  /*
  Contains application info. This is actually not that important.
  The only real important field is apiVersion.
  */
  VkApplicationInfo applicationInfo = {};
  applicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.pApplicationName   = "Hello world app";
  applicationInfo.applicationVersion = 0;
  applicationInfo.pEngineName        = "awesomeengine";
  applicationInfo.engineVersion      = 0;
  applicationInfo.apiVersion         = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.flags = 0;
  createInfo.pApplicationInfo = &applicationInfo;

  // Give our desired layers and extensions to vulkan.
  createInfo.enabledLayerCount       = uint32_t(a_enabledLayers.size());
  createInfo.ppEnabledLayerNames     = a_enabledLayers.data();
  createInfo.enabledExtensionCount   = uint32_t(enabledExtensions.size());
  createInfo.ppEnabledExtensionNames = enabledExtensions.data();

  /*
  Actually create the instance.
  Having created the instance, we can actually start using vulkan.
  */
  VkInstance instance;
  VK_CHECK_RESULT(vkCreateInstance(&createInfo, NULL, &instance));

  return instance;
}


void vk_utils::InitDebugReportCallback(VkInstance a_instance, DebugReportCallbackFuncType a_callback, VkDebugReportCallbackEXT* a_debugReportCallback)
{
  // Register a callback function for the extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME, so that warnings emitted from the validation
  // layer are actually printed.

  VkDebugReportCallbackCreateInfoEXT createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
  createInfo.pfnCallback = a_callback;

  // We have to explicitly load this function.
  //
  auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(a_instance, "vkCreateDebugReportCallbackEXT");
  if (vkCreateDebugReportCallbackEXT == nullptr)
    RUN_TIME_ERROR("Could not load vkCreateDebugReportCallbackEXT");

  // Create and register callback.
  VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(a_instance, &createInfo, NULL, a_debugReportCallback));
}

// bool isDeviceSuitable(VkPhysicalDevice device)
// {
//   QueueFamilyIndices indices = findQueueFamilies(device);
// 
//   bool extensionsSupported = checkDeviceExtensionSupport(device);
// 
//   bool swapChainAdequate = false;
//   if (extensionsSupported) {
//     SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
//     swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
//   }
// 
//   return indices.isComplete() && extensionsSupported && swapChainAdequate;
// }

VkPhysicalDevice vk_utils::FindPhysicalDevice(VkInstance a_instance, bool a_printInfo, int a_preferredDeviceId)
{
  /*
  In this function, we find a physical device that can be used with Vulkan.
  */

  /*
  So, first we will list all physical devices on the system with vkEnumeratePhysicalDevices .
  */
  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(a_instance, &deviceCount, NULL);
  if (deviceCount == 0) {
    RUN_TIME_ERROR("vk_utils::FindPhysicalDevice, no Vulkan devices found");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(a_instance, &deviceCount, devices.data());

  /*
  Next, we choose a device that can be used for our purposes.

  With VkPhysicalDeviceFeatures(), we can retrieve a fine-grained list of physical features supported by the device.
  However, in this demo, we are simply launching a simple compute shader, and there are no
  special physical features demanded for this task.

  With VkPhysicalDeviceProperties(), we can obtain a list of physical device properties. Most importantly,
  we obtain a list of physical device limitations. For this application, we launch a compute shader,
  and the maximum size of the workgroups and total number of compute shader invocations is limited by the physical device,
  and we should ensure that the limitations named maxComputeWorkGroupCount, maxComputeWorkGroupInvocations and
  maxComputeWorkGroupSize are not exceeded by our application.  Moreover, we are using a storage bufferStaging in the compute shader,
  and we should ensure that it is not larger than the device can handle, by checking the limitation maxStorageBufferRange.

  However, in our application, the workgroup size and total number of shader invocations is relatively small, and the storage bufferStaging is
  not that large, and thus a vast majority of devices will be able to handle it. This can be verified by looking at some devices at_
  http://vulkan.gpuinfo.org/

  Therefore, to keep things simple and clean, we will not perform any such checks here, and just pick the first physical
  device in the list. But in a real and serious application, those limitations should certainly be taken into account.

  */
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

  if(a_printInfo)
    std::cout << "FindPhysicalDevice: { " << std::endl;

  VkPhysicalDeviceProperties props;
  VkPhysicalDeviceFeatures   features;

  for (int i=0;i<devices.size();i++)
  {
    vkGetPhysicalDeviceProperties(devices[i], &props);
    vkGetPhysicalDeviceFeatures(devices[i], &features);

    if (a_printInfo)
      std::cout << "  device " << i << ", name = " << props.deviceName;

    if (i == a_preferredDeviceId) // && isDeviceSuitable(device)
    {
      physicalDevice = devices[i];
      std::cout << " <-- (selected)" << std::endl;
    }

    std::cout << std::endl;
  }
  if(a_printInfo)
    std::cout << "}" << std::endl;

  // try to select some device if preferred was not selected
  //
  if(physicalDevice == VK_NULL_HANDLE)
  {
    for (int i=0;i<devices.size();i++)
    {
      if(true)
      {
        physicalDevice = devices[i];
        break;
      }
    }
  }

  if(physicalDevice == VK_NULL_HANDLE)
    RUN_TIME_ERROR("vk_utils::FindPhysicalDevice, no Vulkan devices with compute capability found");

  return physicalDevice;
}

uint32_t vk_utils::GetComputeQueueFamilyIndex(VkPhysicalDevice a_physicalDevice)
{
  return vk_utils::GetQueueFamilyIndex(a_physicalDevice, VK_QUEUE_COMPUTE_BIT);
}

uint32_t vk_utils::GetQueueFamilyIndex(VkPhysicalDevice a_physicalDevice, VkQueueFlagBits a_bits)
{
  uint32_t queueFamilyCount;

  vkGetPhysicalDeviceQueueFamilyProperties(a_physicalDevice, &queueFamilyCount, NULL);

  // Retrieve all queue families.
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(a_physicalDevice, &queueFamilyCount, queueFamilies.data());

  // Now find a family that supports compute.
  uint32_t i = 0;
  for (; i < queueFamilies.size(); ++i)
  {
    VkQueueFamilyProperties props = queueFamilies[i];

    if (props.queueCount > 0 && (props.queueFlags & a_bits))  // found a queue with compute. We're done!
      break;
  }

  if (i == queueFamilies.size())
    RUN_TIME_ERROR(" vk_utils::GetComputeQueueFamilyIndex: could not find a queue family that supports operations");

  return i;
}


VkDevice vk_utils::CreateLogicalDevice(uint32_t queueFamilyIndex, VkPhysicalDevice physicalDevice, const std::vector<const char *>& a_enabledLayers, std::vector<const char *> a_extentions)
{
  // When creating the device, we also specify what queues it has.
  //
  VkDeviceQueueCreateInfo queueCreateInfo = {};
  queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
  queueCreateInfo.queueCount       = 1;    // create one queue in this family. We don't need more.
  float queuePriorities            = 1.0;  // we only have one queue, so this is not that imporant.
  queueCreateInfo.pQueuePriorities = &queuePriorities;

  // Now we create the logical device. The logical device allows us to interact with the physical device.
  //
  VkDeviceCreateInfo deviceCreateInfo = {};

  // Specify any desired device features here. We do not need any for this application, though.
  //
  VkPhysicalDeviceFeatures deviceFeatures = {};
  deviceFeatures.fillModeNonSolid = VK_TRUE;   // me want to darw lines also

  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.enabledLayerCount    = uint32_t(a_enabledLayers.size());  // need to specify validation layers here as well.
  deviceCreateInfo.ppEnabledLayerNames  = a_enabledLayers.data();
  deviceCreateInfo.pQueueCreateInfos    = &queueCreateInfo;        // when creating the logical device, we also specify what queues it has.
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pEnabledFeatures     = &deviceFeatures;
  deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(a_extentions.size());
  deviceCreateInfo.ppEnabledExtensionNames = a_extentions.data();

  VkDevice device;
  VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device)); // create logical device.

  return device;
}


uint32_t vk_utils::FindMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice)
{
  VkPhysicalDeviceMemoryProperties memoryProperties;

  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

  /*
  How does this search work?
  See the documentation of VkPhysicalDeviceMemoryProperties for a detailed description.
  */
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
  {
    if ((memoryTypeBits & (1 << i)) &&
        ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
      return i;
  }

  return -1;
}

std::vector<uint32_t> vk_utils::ReadFile(const char* filename)
{
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL)
  {
    std::string errorMsg = std::string("[vk_utils::ReadFile]: can't open file ") + std::string(filename);
    RUN_TIME_ERROR(errorMsg.c_str());
  }

  // get file size.
  fseek(fp, 0, SEEK_END);
  long filesize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  long filesizepadded = long(ceil(filesize / 4.0)) * 4;

  std::vector<uint32_t> resData(filesizepadded/4);

  // read file contents.
  char *str = (char*)resData.data();
  fread(str, filesize, sizeof(char), fp);
  fclose(fp);

  // data padding.
  for (int i = filesize; i < filesizepadded; i++)
    str[i] = 0;

  return resData;
}

VkShaderModule vk_utils::CreateShaderModule(VkDevice a_device, const std::vector<uint32_t>& code)
{
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size() * sizeof(uint32_t);
  createInfo.pCode = code.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(a_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    throw std::runtime_error("[CreateShaderModule]: failed to create shader module!");

  return shaderModule;
}

VkCommandPool vk_utils::CreateCommandPool(VkDevice a_device, VkPhysicalDevice a_physDevice, VkQueueFlagBits a_queueFlags, VkCommandPoolCreateFlagBits a_poolFlags)
{
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags            = a_poolFlags;
  poolInfo.queueFamilyIndex = vk_utils::GetQueueFamilyIndex(a_physDevice, a_queueFlags);

  VkCommandPool commandPool;
  if (vkCreateCommandPool(a_device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    throw std::runtime_error("[CreateCommandPool]: failed to create command pool!");

  return commandPool;
}

std::vector<VkCommandBuffer> vk_utils::CreateCommandBuffers(VkDevice a_device, VkCommandPool a_pool, uint32_t a_buffNum)
{
  std::vector<VkCommandBuffer> commandBuffers(a_buffNum);

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = a_pool;
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = a_buffNum;

  if (vkAllocateCommandBuffers(a_device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    throw std::runtime_error("[CreateCommandPoolAndBuffers]: failed to allocate command buffers!");

  return commandBuffers;
}

void vk_utils::ExecuteCommandBufferNow(VkCommandBuffer a_cmdBuff, VkQueue a_queue, VkDevice a_device)
{
  // Now we shall finally submit the recorded command bufferStaging to a queue.
  //
  VkSubmitInfo submitInfo       = {};
  submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1; // submit a single command bufferStaging
  submitInfo.pCommandBuffers    = &a_cmdBuff; // the command bufferStaging to submit.

  VkFence fence;
  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = 0;
  VK_CHECK_RESULT(vkCreateFence(a_device, &fenceCreateInfo, NULL, &fence));

  // We submit the command bufferStaging on the queue, at the same time giving a fence.
  //
  VK_CHECK_RESULT(vkQueueSubmit(a_queue, 1, &submitInfo, fence));

  // The command will not have finished executing until the fence is signalled.
  // So we wait here. We will directly after this read our bufferStaging from the GPU,
  // and we will not be sure that the command has finished executing unless we wait for the fence.
  // Hence, we use a fence here.
  //
  VK_CHECK_RESULT(vkWaitForFences(a_device, 1, &fence, VK_TRUE, 100000000000));

  vkDestroyFence(a_device, fence, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SwapChainSupportDetails 
{
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR a_surface) 
{
  SwapChainSupportDetails details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, a_surface, &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, a_surface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, a_surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, a_surface, &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, a_surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) 
{
  for (const auto& availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) 
{
  for (const auto& availablePresentMode : availablePresentModes) 
  {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
      return availablePresentMode;
    else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
      return availablePresentMode;
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int a_width, int a_height) 
{
  if (capabilities.currentExtent.width != UINT32_MAX) 
  {
    return capabilities.currentExtent;
  }
  else 
  {
    VkExtent2D actualExtent = { uint32_t(a_width), uint32_t(a_height) };

    actualExtent.width  = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

    return actualExtent;
  }
}


void vk_utils::CreateCwapChain(VkPhysicalDevice a_physDevice, VkDevice a_device, VkSurfaceKHR a_surface, int a_width, int a_height,
                               ScreenBufferResources* a_buff)
{
  SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(a_physDevice, a_surface);

  VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode     = ChooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D extent                = ChooseSwapExtent(swapChainSupport.capabilities, a_width, a_height);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo = {};
  createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface          = a_surface;
  createInfo.minImageCount    = imageCount;
  createInfo.imageFormat      = surfaceFormat.format;
  createInfo.imageColorSpace  = surfaceFormat.colorSpace;
  createInfo.imageExtent      = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  createInfo.preTransform     = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode      = presentMode;
  createInfo.clipped          = VK_TRUE;
  createInfo.oldSwapchain     = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(a_device, &createInfo, nullptr, &a_buff->swapChain) != VK_SUCCESS)
    throw std::runtime_error("[vk_utils::CreateCwapChain]: failed to create swap chain!");

  vkGetSwapchainImagesKHR(a_device, a_buff->swapChain, &imageCount, nullptr);
  a_buff->swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(a_device, a_buff->swapChain, &imageCount, a_buff->swapChainImages.data());

  a_buff->swapChainImageFormat = surfaceFormat.format;
  a_buff->swapChainExtent      = extent;
}

void vk_utils::CreateScreenImageViews(VkDevice a_device, ScreenBufferResources* pScreen)
{
  pScreen->swapChainImageViews.resize(pScreen->swapChainImages.size());

  for (size_t i = 0; i < pScreen->swapChainImages.size(); i++)
  {
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image        = pScreen->swapChainImages[i];
    createInfo.viewType     = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format       = pScreen->swapChainImageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel   = 0;
    createInfo.subresourceRange.levelCount     = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(a_device, &createInfo, nullptr, &pScreen->swapChainImageViews[i]) != VK_SUCCESS)
      throw std::runtime_error("[vk_utils::CreateImageViews]: failed to create image views!");
  }

}

void vk_utils::CreateScreenFrameBuffers(VkDevice a_device, VkRenderPass a_renderPass, VkImageView a_depthView, ScreenBufferResources* pScreen)
{
  pScreen->swapChainFramebuffers.resize(pScreen->swapChainImageViews.size());

  for (size_t i = 0; i < pScreen->swapChainImageViews.size(); i++) 
  {
    VkImageView attachments[] = { pScreen->swapChainImageViews[i], a_depthView };

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = a_renderPass;
    framebufferInfo.attachmentCount = 2;
    framebufferInfo.pAttachments    = attachments;
    framebufferInfo.width           = pScreen->swapChainExtent.width;
    framebufferInfo.height          = pScreen->swapChainExtent.height;
    framebufferInfo.layers          = 1;

    if (vkCreateFramebuffer(a_device, &framebufferInfo, nullptr, &pScreen->swapChainFramebuffers[i]) != VK_SUCCESS)
      throw std::runtime_error("failed to create framebuffer!");
  }
}

void vk_utils::CreateDepthTexture(VkDevice a_device, VkPhysicalDevice a_physDevice, const int a_width, const int a_height,
                                  VkDeviceMemory *a_pImageMemory, VkImage *a_image, VkImageView* a_imageView)
{
  VkImageCreateInfo imgCreateInfo = {};
  imgCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgCreateInfo.pNext         = nullptr;
  imgCreateInfo.flags         = 0; // not sure about this ...
  imgCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
  imgCreateInfo.format        = VK_FORMAT_D32_SFLOAT;
  imgCreateInfo.extent        = VkExtent3D{uint32_t(a_width), uint32_t(a_height), 1};
  imgCreateInfo.mipLevels     = 1;
  imgCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
  imgCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
  imgCreateInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  imgCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imgCreateInfo.arrayLayers   = 1;
  VK_CHECK_RESULT(vkCreateImage(a_device, &imgCreateInfo, nullptr, a_image));

  VkMemoryRequirements memoryRequirements;
  vkGetImageMemoryRequirements(a_device, (*a_image), &memoryRequirements);
  
  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.allocationSize  = memoryRequirements.size; // specify required memory.
  allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, a_physDevice);
  VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, NULL, a_pImageMemory)); // allocate memory on device.
  VK_CHECK_RESULT(vkBindImageMemory(a_device, (*a_image), (*a_pImageMemory), 0));

  VkImageViewCreateInfo imageViewInfo = {};

  imageViewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewInfo.flags    = 0;
  imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageViewInfo.format   = VK_FORMAT_D32_SFLOAT;

  imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
  imageViewInfo.subresourceRange.baseMipLevel   = 0;
  imageViewInfo.subresourceRange.baseArrayLayer = 0;
  imageViewInfo.subresourceRange.layerCount     = 1;
  imageViewInfo.subresourceRange.levelCount     = 1;
  imageViewInfo.image                           = (*a_image);
    
  VK_CHECK_RESULT(vkCreateImageView(a_device, &imageViewInfo, nullptr, a_imageView));
}

void vk_utils::CreateRenderPass(VkDevice a_device, RenderTargetInfo2D a_rtInfo,
                                VkRenderPass* a_pRenderPass)
{
  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format         = a_rtInfo.fmt;
  colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp         = a_rtInfo.loadOp;
  colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout  = a_rtInfo.initialLayout;           //  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ? 
  colorAttachment.finalLayout    = a_rtInfo.finalLayout;

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments    = &colorAttachmentRef;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass    = 0;
  dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments    = &colorAttachment;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies   = &dependency;

  if (vkCreateRenderPass(a_device, &renderPassInfo, nullptr, a_pRenderPass) != VK_SUCCESS)
    throw std::runtime_error("[CreateRenderPass]: failed to create render pass!");
}

bool vk_utils::IsDepthFormat(VkFormat a_format)
{
  return (a_format == VK_FORMAT_D32_SFLOAT) || (a_format == VK_FORMAT_D16_UNORM);
}
