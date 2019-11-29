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
   
    VkMemoryRequirements CreateR8G8B8A8(VkDevice a_device, const int a_width, const int a_height);
    void                 BindMemory    (VkDeviceMemory a_memStorage, size_t a_offset);
    void                 UpdateNow     (const unsigned int* a_data, const int a_width, const int a_height);

    VkImage              Image()   const { return imageGPU; }
    VkImageView          View()    const { return imageView; }
    VkSampler            Sampler() const { return imageSampler; }

  protected:

    void CreateOther();

    VkDeviceMemory memStorage; // this is bad design in fact, you should store memory somewhere else and/or use memory pools;
    VkImage        imageGPU;
    VkSampler      imageSampler;
    VkImageView    imageView;
    VkDevice       m_device;
  };
};

#endif