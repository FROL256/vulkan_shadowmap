#ifndef VULKAN_TEXTURE_HELPER_H
#define VULKAN_TEXTURE_HELPER_H

#include <vulkan/vulkan.h>
#include <vector>

#include <stdexcept>
#include <sstream>
#include <memory>

namespace vk_texture
{

  struct SimpleVulkanTexture
  {
    SimpleVulkanTexture() : memStorage(0), imageGPU(0), imageSampler(0), imageView(0) {}
    ~SimpleVulkanTexture();
   
    VkMemoryRequirements CreateImage(VkDevice a_device, const int a_width, const int a_height, VkFormat a_format);
    void                 BindMemory (VkDeviceMemory a_memStorage, size_t a_offset);

    void                 GenerateMips(VkCommandBuffer a_cmdBuff, VkQueue a_queue);

    VkImage              Image()   const { return imageGPU; }
    VkImageView          View()    const { return imageView; }
    VkSampler            Sampler() const { return imageSampler; }

  protected:

    void CreateOther();

    VkDeviceMemory memStorage; // SimpleVulkanTexture DOES NOT OWN memStorage! It just save reference to it.
    VkImage        imageGPU;
    VkSampler      imageSampler;
    VkImageView    imageView;
    VkDevice       m_device;
    VkFormat       m_format;
    int m_width, m_height;
    int m_mipLevels;
  };
};

#endif