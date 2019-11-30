#include "vk_texture.h"
#include "vk_utils.h"
#include <cassert>

vk_texture::SimpleVulkanTexture::~SimpleVulkanTexture()
{
  if (m_device == nullptr)
    return;

  vkDestroyImage    (m_device, imageGPU, NULL);     imageGPU = nullptr;
  vkDestroyImageView(m_device, imageView, NULL);    imageView = nullptr;
  vkDestroySampler  (m_device, imageSampler, NULL); imageSampler = nullptr;
}

VkMemoryRequirements vk_texture::SimpleVulkanTexture::CreateImage(VkDevice a_device, const int a_width, const int a_height, VkFormat a_format)
{
  m_device = a_device;
  m_format = a_format;

  VkImageCreateInfo imgCreateInfo = {};
  imgCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgCreateInfo.pNext         = nullptr;
  imgCreateInfo.flags         = 0; // not sure about this ...
  imgCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
  imgCreateInfo.format        = a_format;
  imgCreateInfo.extent        = VkExtent3D{ uint32_t(a_width), uint32_t(a_height), 1 };
  imgCreateInfo.mipLevels     = 1;
  imgCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
  imgCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
  imgCreateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // copy to the texture and read then
  imgCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imgCreateInfo.arrayLayers   = 1;

  VK_CHECK_RESULT(vkCreateImage(a_device, &imgCreateInfo, nullptr, &imageGPU));

  VkMemoryRequirements memoryRequirements;
  vkGetImageMemoryRequirements(a_device, imageGPU, &memoryRequirements);

  VkSamplerCreateInfo samplerInfo = {};
  {
    samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext            = nullptr;
    samplerInfo.flags            = 0;
    samplerInfo.magFilter        = VK_FILTER_LINEAR;
    samplerInfo.minFilter        = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.compareOp        = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod           = 0;
    samplerInfo.maxLod           = 0;
    samplerInfo.maxAnisotropy    = 1.0;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
  }
  VK_CHECK_RESULT(vkCreateSampler(a_device, &samplerInfo, nullptr, &this->imageSampler));

  return memoryRequirements;
}

void vk_texture::SimpleVulkanTexture::BindMemory(VkDeviceMemory a_memStorage, size_t a_offset)
{
  assert(memStorage == nullptr); // this implementation does not allow to rebind memory!
  assert(imageView  == nullptr); // may be later ... 

  memStorage = a_memStorage;

  VK_CHECK_RESULT(vkBindImageMemory(m_device, imageGPU, a_memStorage, a_offset));

  VkImageViewCreateInfo imageViewInfo = {};
  {
    imageViewInfo.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.flags      = 0;
    imageViewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format     = m_format;
    imageViewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    // The subresource range describes the set of mip levels (and array layers) that can be accessed through this image view
    // It's possible to create multiple image views for a single image referring to different (and/or overlapping) ranges of the image
    imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel   = 0;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount     = 1;
    imageViewInfo.subresourceRange.levelCount     = 1;
    // The view will be based on the texture's image
    imageViewInfo.image = this->imageGPU;
  }
  VK_CHECK_RESULT(vkCreateImageView(m_device, &imageViewInfo, nullptr, &this->imageView));
}

void vk_texture::SimpleVulkanTexture::CreateOther()
{
  VkSamplerCreateInfo samplerInfo = {};
  {
    samplerInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext         = nullptr;
    samplerInfo.flags         = 0;
    samplerInfo.magFilter     = VK_FILTER_LINEAR;
    samplerInfo.minFilter     = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.mipLodBias    = 0.0f;
    samplerInfo.compareOp     = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod        = 0;
    samplerInfo.maxLod        = 0;
    samplerInfo.maxAnisotropy = 1.0;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
  }
  VK_CHECK_RESULT(vkCreateSampler(m_device, &samplerInfo, nullptr, &this->imageSampler));

 
}

