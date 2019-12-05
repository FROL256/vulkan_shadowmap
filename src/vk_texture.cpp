#include "vk_texture.h"
#include "vk_utils.h"
#include <cassert>
#include <cmath>
#include <algorithm>

#ifdef WIN32
#undef max
#undef min
#endif

vk_texture::SimpleTexture2D::~SimpleTexture2D()
{
  if (m_device == nullptr)
    return;

  vkDestroyImage    (m_device, m_image, NULL);     m_image = nullptr;
  vkDestroyImageView(m_device, m_view, NULL);    m_view = nullptr;
  vkDestroySampler  (m_device, m_sampler, NULL); m_sampler = nullptr;

  m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  m_currentStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
}

VkMemoryRequirements vk_texture::SimpleTexture2D::CreateImage(VkDevice a_device, const int a_width, const int a_height, VkFormat a_format)
{
  m_device = a_device;
  m_width  = a_width;
  m_height = a_height;
  m_format = a_format;

  m_mipLevels = int(floor(log2(std::max(m_width, m_height))) + 1);

  VkImageCreateInfo imgCreateInfo = {};
  imgCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgCreateInfo.pNext         = nullptr;
  imgCreateInfo.flags         = 0; // not sure about this ...
  imgCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
  imgCreateInfo.format        = a_format;
  imgCreateInfo.extent        = VkExtent3D{ uint32_t(a_width), uint32_t(a_height), 1 };
  imgCreateInfo.mipLevels     = m_mipLevels;
  imgCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
  imgCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
  imgCreateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT| VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // copy to the texture and read then
  imgCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imgCreateInfo.arrayLayers   = 1;

  VK_CHECK_RESULT(vkCreateImage(a_device, &imgCreateInfo, nullptr, &m_image));

  VkMemoryRequirements memoryRequirements;
  vkGetImageMemoryRequirements(a_device, m_image, &memoryRequirements);

  VkSamplerCreateInfo samplerInfo = {};
  {
    samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext            = nullptr;
    samplerInfo.flags            = 0;
    samplerInfo.magFilter        = VK_FILTER_LINEAR;
    samplerInfo.minFilter        = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.compareOp        = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod           = 0;
    samplerInfo.maxLod           = float(m_mipLevels);
    samplerInfo.maxAnisotropy    = 1.0;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
  }
  VK_CHECK_RESULT(vkCreateSampler(a_device, &samplerInfo, nullptr, &this->m_sampler));

  m_createImageInfo = imgCreateInfo;

  return memoryRequirements;
}

//VK_IMAGE_ASPECT_DEPTH_BIT 

static void BindImageToMemoryAndCreateImageView(VkDevice a_device, VkImage a_image, VkFormat a_format, uint32_t a_mipLevels, 
                                                VkDeviceMemory a_memStorage, size_t a_offset,
                                                VkImageView* a_pView)
{
  VK_CHECK_RESULT(vkBindImageMemory(a_device, a_image, a_memStorage, a_offset));

  const bool isDepthTexture = vk_utils::IsDepthFormat(a_format);

  VkImageViewCreateInfo imageViewInfo = {};
  {
    imageViewInfo.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.flags      = 0;
    imageViewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format     = a_format;
    imageViewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    imageViewInfo.subresourceRange.aspectMask     = isDepthTexture ? VK_IMAGE_ASPECT_DEPTH_BIT :  VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel   = 0;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount     = 1;
    imageViewInfo.subresourceRange.levelCount     = a_mipLevels;
    imageViewInfo.image = a_image;     // The view will be based on the texture's image
  }
  VK_CHECK_RESULT(vkCreateImageView(a_device, &imageViewInfo, nullptr, a_pView));
}

void vk_texture::SimpleTexture2D::BindMemory(VkDeviceMemory a_memStorage, size_t a_offset)
{
  assert(m_memStorage == nullptr); // this implementation does not allow to rebind memory!
  assert(m_view       == nullptr); // may be later ... 

  m_memStorage = a_memStorage;

  BindImageToMemoryAndCreateImageView(m_device, m_image, m_format, m_mipLevels,
                                      a_memStorage, a_offset,
                                      &m_view);
}

void vk_texture::SimpleTexture2D::Update(const void* a_src, int a_width, int a_height, int a_bpp, ICopyEngine* a_pCopyImpl)
{
  assert(a_pCopyImpl != nullptr);
  
  a_pCopyImpl->UpdateImage(Image(), a_src, a_width, a_height, a_bpp);
  
  m_currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; 
  m_currentStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
}

namespace vk_utils
{
    static void setImageLayout(
            VkCommandBuffer cmdbuffer,
            VkImage image,
            VkImageLayout oldImageLayout,
            VkImageLayout newImageLayout,
            VkImageSubresourceRange subresourceRange,
            VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask)
    {
      // Create an image barrier object
      VkImageMemoryBarrier imageMemoryBarrier = {};
      imageMemoryBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      imageMemoryBarrier.oldLayout           = oldImageLayout;
      imageMemoryBarrier.newLayout           = newImageLayout;
      imageMemoryBarrier.image               = image;
      imageMemoryBarrier.subresourceRange    = subresourceRange;

      // Source layouts (old)
      // Source access mask controls actions that have to be finished on the old layout
      // before it will be transitioned to the new layout
      switch (oldImageLayout)
      {
        case VK_IMAGE_LAYOUT_UNDEFINED:
          // Image layout is undefined (or does not matter)
          // Only valid as initial layout
          // No flags required, listed only for completeness
          imageMemoryBarrier.srcAccessMask = 0;
          break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
          // Image is preinitialized
          // Only valid as initial layout for linear images, preserves memory contents
          // Make sure host writes have been finished
          imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
          break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
          // Image is a color attachment
          // Make sure any writes to the color buffer have been finished
          imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
          break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
          // Image is a depth/stencil attachment
          // Make sure any writes to the depth/stencil buffer have been finished
          imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
          break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
          // Image is a transfer source
          // Make sure any reads from the image have been finished
          imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
          break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
          // Image is a transfer destination
          // Make sure any writes to the image have been finished
          imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
          // Image is read by a shader
          // Make sure any shader reads from the image have been finished
          imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
          break;
        default:
          // Other source layouts aren't handled (yet)
          break;
      }

      // Target layouts (new)
      // Destination access mask controls the dependency for the new image layout
      switch (newImageLayout)
      {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
          // Image will be used as a transfer destination
          // Make sure any writes to the image have been finished
          imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
          // Image will be used as a transfer source
          // Make sure any reads from the image have been finished
          imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
          break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
          // Image will be used as a color attachment
          // Make sure any writes to the color buffer have been finished
          imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
          break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
          // Image layout will be used as a depth/stencil attachment
          // Make sure any writes to depth/stencil buffer have been finished
          imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
          break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
          // Image will be read in a shader (sampler, input attachment)
          // Make sure any writes to the image have been finished
          if (imageMemoryBarrier.srcAccessMask == 0)
          {
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
          }
          imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
          break;
        default:
          // Other source layouts aren't handled (yet)
          break;
      }

      // Put barrier inside setup command buffer
      vkCmdPipelineBarrier(
              cmdbuffer,
              srcStageMask,
              dstStageMask,
              0,
              0, nullptr,
              0, nullptr,
              1, &imageMemoryBarrier);
    }
};

void vk_texture::SimpleTexture2D::GenerateMipsCmd(VkCommandBuffer a_cmdBuff)
{
  VkCommandBuffer blitCmd = a_cmdBuff;

  // at first, transfer 0 mip level to the VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  {
    VkImageMemoryBarrier imgBar = {};
  
    imgBar.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgBar.pNext               = nullptr;
    imgBar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgBar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    
    imgBar.srcAccessMask = 0;
    imgBar.dstAccessMask = 0;
    imgBar.oldLayout     = m_currentLayout;
    imgBar.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imgBar.image         = m_image;
  
    imgBar.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imgBar.subresourceRange.baseMipLevel   = 0;
    imgBar.subresourceRange.levelCount     = 1;
    imgBar.subresourceRange.baseArrayLayer = 0;
    imgBar.subresourceRange.layerCount     = 1;
  
    vkCmdPipelineBarrier(blitCmd,
                         m_currentStage,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &imgBar);
  }


  // Copy down mips from n-1 to n
  for (int32_t i = 1; i < m_mipLevels; i++)
  {
    VkImageBlit imageBlit{};

    // Source
    imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBlit.srcSubresource.layerCount = 1;
    imageBlit.srcSubresource.mipLevel   = i - 1;
    imageBlit.srcOffsets[1].x           = int32_t(m_width >> (i - 1));
    imageBlit.srcOffsets[1].y           = int32_t(m_height >> (i - 1));
    imageBlit.srcOffsets[1].z           = 1;

    // Destination
    imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBlit.dstSubresource.layerCount = 1;
    imageBlit.dstSubresource.mipLevel   = i;
    imageBlit.dstOffsets[1].x           = int32_t(m_width >> i);
    imageBlit.dstOffsets[1].y           = int32_t(m_height >> i);
    imageBlit.dstOffsets[1].z           = 1;

    VkImageSubresourceRange mipSubRange = {};
    mipSubRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
    mipSubRange.baseMipLevel = i;
    mipSubRange.levelCount   = 1;
    mipSubRange.layerCount   = 1;

    // Transition current mip level to transfer dest
    vk_utils::setImageLayout(
            blitCmd,
            m_image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            mipSubRange,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Blit from previous level
    vkCmdBlitImage(
            blitCmd,
            m_image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageBlit,
            VK_FILTER_LINEAR);

    // Transition current mip level to transfer source for read in next iteration
    vk_utils::setImageLayout(
            blitCmd,
            m_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            mipSubRange,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);
  }
  
  //m_currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  //this->ChangeLayoutCmd(blitCmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

  // After the loop, all mip layers are in TRANSFER_SRC layout, so transition all to SHADER_READ
  {
    VkImageMemoryBarrier imgBar = {};
  
    imgBar.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgBar.pNext               = nullptr;
    imgBar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgBar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imgBar.srcAccessMask = 0;
    imgBar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imgBar.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imgBar.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgBar.image         = m_image;
  
    imgBar.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imgBar.subresourceRange.baseMipLevel   = 0;
    imgBar.subresourceRange.levelCount     = m_mipLevels;
    imgBar.subresourceRange.baseArrayLayer = 0;
    imgBar.subresourceRange.layerCount     = 1;
  
    vkCmdPipelineBarrier(blitCmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &imgBar);
  }

  m_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  m_currentStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  
}

void vk_texture::SimpleTexture2D::ChangeLayoutCmd(VkCommandBuffer a_cmdBuff, VkImageLayout a_newLayout, VkPipelineStageFlags a_newStage)
{
  if(m_currentLayout == a_newLayout && m_currentStage == a_newStage)
    return;

  VkImageMemoryBarrier imgBar = {};
  
  imgBar.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imgBar.pNext               = nullptr;
  imgBar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imgBar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imgBar.srcAccessMask       = 0;                                        // #NOTE: THIS IS NOT CORRECT! please use vk_utils::setImageLayout!
  imgBar.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;                // #NOTE: THIS IS NOT CORRECT! please use vk_utils::setImageLayout!
  imgBar.oldLayout           = m_currentLayout;
  imgBar.newLayout           = a_newLayout;
  imgBar.image               = m_image;

  imgBar.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  imgBar.subresourceRange.baseMipLevel   = 0;
  imgBar.subresourceRange.levelCount     = m_mipLevels;
  imgBar.subresourceRange.baseArrayLayer = 0;
  imgBar.subresourceRange.layerCount     = 1;

  vkCmdPipelineBarrier(a_cmdBuff,
                       m_currentStage,
                       a_newStage,
                       0,
                       0, nullptr,
                       0, nullptr,
                       1, &imgBar);

  m_currentLayout = a_newLayout;
  m_currentStage  = a_newStage;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vk_texture::RenderableTexture2D::~RenderableTexture2D()
{
  if (m_device == nullptr)
    return;

  //vkDestroyPipeline(m_device, m_pipeline, nullptr);
  //vkDestroyPipelineLayout(m_device, m_layout, nullptr);
  vkDestroyRenderPass (m_device, m_renderPass, nullptr);
  vkDestroyFramebuffer(m_device, m_fbo, NULL);


  vkDestroyImage      (m_device, m_image, NULL);   m_image   = nullptr;
  vkDestroyImageView  (m_device, m_view, NULL);    m_view    = nullptr;
  vkDestroySampler    (m_device, m_sampler, NULL); m_sampler = nullptr;

  m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  m_currentStage  = 0;
}


VkMemoryRequirements vk_texture::RenderableTexture2D::CreateImage(VkDevice a_device, const int a_width, const int a_height, VkFormat a_format)
{
  m_device = a_device;
  m_width  = a_width;
  m_height = a_height;
  m_format = a_format;

  m_mipLevels = 1;

  const bool isDepthTexture  = vk_utils::IsDepthFormat(a_format);
  const auto attachmentFlags = isDepthTexture ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo imgCreateInfo = {};
  imgCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgCreateInfo.pNext         = nullptr;
  imgCreateInfo.flags         = 0; // not sure about this ...
  imgCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
  imgCreateInfo.format        = a_format;
  imgCreateInfo.extent        = VkExtent3D{ uint32_t(a_width), uint32_t(a_height), 1 };
  imgCreateInfo.mipLevels     = m_mipLevels;
  imgCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
  imgCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
  imgCreateInfo.usage         = attachmentFlags | VK_IMAGE_USAGE_SAMPLED_BIT; // copy to the texture and read then
  imgCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imgCreateInfo.arrayLayers   = 1;

  VK_CHECK_RESULT(vkCreateImage(a_device, &imgCreateInfo, nullptr, &m_image));

  VkMemoryRequirements memoryRequirements;
  vkGetImageMemoryRequirements(a_device, m_image, &memoryRequirements);

  VkSamplerCreateInfo samplerInfo = {};
  {
    samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext            = nullptr;
    samplerInfo.flags            = 0;
    samplerInfo.magFilter        = VK_FILTER_LINEAR;
    samplerInfo.minFilter        = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU     = isDepthTexture ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV     = isDepthTexture ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;;
    samplerInfo.addressModeW     = isDepthTexture ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.compareOp        = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod           = 0;
    samplerInfo.maxLod           = float(m_mipLevels);
    samplerInfo.maxAnisotropy    = 1.0;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor      = isDepthTexture ? VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK : VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
  }
  VK_CHECK_RESULT(vkCreateSampler(a_device, &samplerInfo, nullptr, &this->m_sampler));

  m_createImageInfo = imgCreateInfo;

  return memoryRequirements;
}

void vk_texture::RenderableTexture2D::CreateRenderPass()
{
  const bool isDepthTexture = vk_utils::IsDepthFormat(m_format);

  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format         = m_format;
  colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout  = isDepthTexture ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout     = isDepthTexture ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;

  if (isDepthTexture)
    subpass.pDepthStencilAttachment = &colorAttachmentRef;
  else
  {
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
  }

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

  if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
    throw std::runtime_error("[CreateRenderPass]: failed to create render pass!");
}

void vk_texture::RenderableTexture2D::BindMemory(VkDeviceMemory a_memStorage, size_t a_offset)
{
  assert(m_memStorage == nullptr); // this implementation does not allow to rebind memory!
  assert(m_view       == nullptr); // may be later ... 

  m_memStorage = a_memStorage;

  BindImageToMemoryAndCreateImageView(m_device, m_image, m_format, m_mipLevels,
                                      a_memStorage, a_offset,
                                      &m_view);
  this->CreateRenderPass();

  // frame buffer objects
  //
  VkImageView attachments[] = { m_view };

  VkFramebufferCreateInfo framebufferInfo = {};
  framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass      = m_renderPass;
  framebufferInfo.attachmentCount = 1;
  framebufferInfo.pAttachments    = attachments;
  framebufferInfo.width           = m_width;
  framebufferInfo.height          = m_height;
  framebufferInfo.layers          = 1;

  if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_fbo) != VK_SUCCESS)
    throw std::runtime_error("[RenderableTexture2D]: failed to create framebuffer!");
}

