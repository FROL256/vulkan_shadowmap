#ifndef VULKAN_COPY_H
#define VULKAN_COPY_H

#include <vulkan/vulkan.h>
#include <vector>

#include <stdexcept>
#include <sstream>

namespace vk_copy
{
  
  struct SimpleCopyHelper
  {
    SimpleCopyHelper(VkPhysicalDevice a_physicalDevice, VkDevice a_device, VkQueue a_transferQueue, size_t a_stagingBuffSize);
    ~SimpleCopyHelper();

    void UpdateBuffer(VkBuffer a_dst, size_t a_dstOffset, const void* a_src, size_t a_size);

  private:

    VkQueue         queue;
    VkCommandPool   cmdPool;
    VkCommandBuffer cmdBuff;

    VkBuffer        stagingBuff;
    VkDeviceMemory  stagingBuffMemory;
    size_t          stagingSize;

    VkPhysicalDevice physDev;
    VkDevice         dev;


    SimpleCopyHelper(const SimpleCopyHelper& rhs) {}
    SimpleCopyHelper& operator=(const SimpleCopyHelper& rhs) { return *this; }
  };

};

#endif