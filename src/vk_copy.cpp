#include "vk_utils.h"
#include "vk_copy.h"

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void CreateStagingBuffer(VkDevice a_device, VkPhysicalDevice a_physDevice, const size_t a_bufferSize,
                                VkBuffer *a_pBuffer, VkDeviceMemory *a_pBufferMemory)
{
  // We will now create a bufferStaging. We will render the mandelbrot set into this bufferStaging
  // in a computer shade later.
  //
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size        = a_bufferSize; // bufferStaging size in bytes.
  bufferCreateInfo.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT; 
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // bufferStaging is exclusive to a single queue family at a time
  VK_CHECK_RESULT(vkCreateBuffer(a_device, &bufferCreateInfo, NULL, a_pBuffer)); // create bufferStaging
  // But the bufferStaging doesn't allocate memory for itself, so we must do that manually.
  // First, we find the memory requirements for the bufferStaging.
  //
  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(a_device, (*a_pBuffer), &memoryRequirements);
    
  // Now use obtained memory requirements info to allocate the memory for the bufferStaging.
  // There are several types of memory that can be allocated, and we must choose a memory type that
  // 1) Satisfies the memory requirements(memoryRequirements.memoryTypeBits).
  // 2) Satifies our own usage requirements. We want to be able to read the bufferStaging memory from the GPU to the CPU
  //    with vkMapMemory, so we set VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT.
  // Also, by setting VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memory written by the device(GPU) will be easily
  // visible to the host(CPU), without having to call any extra flushing commands. So mainly for convenience, we set this flag.
  //
  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.allocationSize  = memoryRequirements.size; // specify required memory.
  allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, a_physDevice);
  VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, NULL, a_pBufferMemory)); // allocate memory on device.
    
  // Now associate that allocated memory with the bufferStaging. With that, the bufferStaging is backed by actual memory.
  VK_CHECK_RESULT(vkBindBufferMemory(a_device, (*a_pBuffer), (*a_pBufferMemory), 0));
}

vk_copy::SimpleCopyHelper::SimpleCopyHelper(VkPhysicalDevice a_physicalDevice, VkDevice a_device, VkQueue a_transferQueue, size_t a_stagingBuffSize)
{
  physDev = a_physicalDevice;
  dev     = a_device;
  queue   = a_transferQueue;

  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = vk_utils::GetQueueFamilyIndex(a_physicalDevice, VK_QUEUE_TRANSFER_BIT);
  if (vkCreateCommandPool(a_device, &poolInfo, nullptr, &cmdPool) != VK_SUCCESS)
    throw std::runtime_error("[vk_utils::SimpleCopyHelper::SimpleCopyHelper]: failed to create command pool!");

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = cmdPool;
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(a_device, &allocInfo, &cmdBuff) != VK_SUCCESS)
    throw std::runtime_error("[CreateCommandPoolAndBuffers]: failed to allocate command buffers!");  

  CreateStagingBuffer(a_device, a_physicalDevice, a_stagingBuffSize, 
                      &stagingBuff, &stagingBuffMemory);

  stagingSize = a_stagingBuffSize;
}

vk_copy::SimpleCopyHelper::~SimpleCopyHelper()
{
  vkDestroyBuffer(dev, stagingBuff, NULL);
  vkFreeMemory   (dev, stagingBuffMemory, NULL);

  vkFreeCommandBuffers(dev, cmdPool, 1, &cmdBuff);
  vkDestroyCommandPool(dev, cmdPool, nullptr);
}


void vk_copy::SimpleCopyHelper::UpdateBuffer(VkBuffer a_dst, size_t a_dstOffset, const void* a_src, size_t a_size)
{
  assert(a_dstOffset % 4 == 0);
  assert(a_size      % 4 == 0);

  if (a_size <= 65536)
  //if(false)
  {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkResetCommandBuffer(cmdBuff, 0);
    vkBeginCommandBuffer(cmdBuff, &beginInfo);
    vkCmdUpdateBuffer   (cmdBuff, a_dst, a_dstOffset, a_size, a_src);
    vkEndCommandBuffer  (cmdBuff);
  }
  else
  {
    if (a_size > stagingSize)
    {
      std::stringstream strOut;
      strOut << "[vk_utils::SimpleCopyHelper::UpdateBuffer]: too large input size " << a_size << ", please allocate larger staging buff";
      throw std::runtime_error(strOut.str().c_str());
    }

    void* mappedMemory = nullptr;
    vkMapMemory(dev, stagingBuffMemory, 0, a_size, 0, &mappedMemory);
    memcpy(mappedMemory, a_src, a_size);
    vkUnmapMemory(dev, stagingBuffMemory);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkResetCommandBuffer(cmdBuff, 0);
    vkBeginCommandBuffer(cmdBuff, &beginInfo);
     
    VkBufferCopy region0 = {};
    region0.srcOffset    = 0;
    region0.dstOffset    = a_dstOffset;
    region0.size         = a_size;

    vkCmdCopyBuffer(cmdBuff, stagingBuff, a_dst, 1, &region0);

    vkEndCommandBuffer(cmdBuff);
  }

  vk_utils::ExecuteCommandBufferNow(cmdBuff, queue, dev);
}

void vk_copy::SimpleCopyHelper::UpdateImage(VkImage a_image, unsigned int* a_src, int a_width, int a_height)
{
  size_t a_size = a_width * a_height * sizeof(unsigned int);

  void* mappedMemory = nullptr;
  vkMapMemory(dev, stagingBuffMemory, 0, a_size, 0, &mappedMemory);
  memcpy(mappedMemory, a_src, a_size);
  vkUnmapMemory(dev, stagingBuffMemory);


  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  vkResetCommandBuffer(cmdBuff, 0);
  vkBeginCommandBuffer(cmdBuff, &beginInfo);

  VkImageMemoryBarrier imgBar = {};
  {
    imgBar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgBar.pNext = nullptr;
    imgBar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgBar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imgBar.srcAccessMask = 0;
    imgBar.dstAccessMask = 0;
    imgBar.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    imgBar.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imgBar.image         = a_image;

    imgBar.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imgBar.subresourceRange.baseMipLevel   = 0;
    imgBar.subresourceRange.levelCount     = 1;
    imgBar.subresourceRange.baseArrayLayer = 0;
    imgBar.subresourceRange.layerCount     = 1;
  };

  vkCmdPipelineBarrier(cmdBuff,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0,
                       0, nullptr,
                       0, nullptr,
                       1, &imgBar);


  VkImageSubresourceLayers shittylayers = {};
  shittylayers.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  shittylayers.mipLevel       = 0;
  shittylayers.baseArrayLayer = 0;
  shittylayers.layerCount     = 1;

  VkBufferImageCopy wholeRegion = {};
  wholeRegion.bufferOffset      = 0;
  wholeRegion.bufferRowLength   = uint32_t(a_width);
  wholeRegion.bufferImageHeight = uint32_t(a_height);
  wholeRegion.imageExtent       = VkExtent3D{ uint32_t(a_width), uint32_t(a_height), 1 };
  wholeRegion.imageOffset       = VkOffset3D{ 0,0,0 };
  wholeRegion.imageSubresource  = shittylayers;

  vkCmdCopyBufferToImage(cmdBuff, stagingBuff, a_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &wholeRegion);

  vkEndCommandBuffer(cmdBuff);

  vk_utils::ExecuteCommandBufferNow(cmdBuff, queue, dev);
}