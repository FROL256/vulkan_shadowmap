#ifndef VULKAN_FSQUAD_H
#define VULKAN_FSQUAD_H

#include <vulkan/vulkan.h>

namespace vk_utils
{
  struct RenderTargetInfo2D
  {
    VkExtent2D         size;
    VkFormat           fmt;
    VkAttachmentLoadOp loadOp;      
    VkImageLayout      initialLayout; 
    VkImageLayout      finalLayout; 
  };

  struct FSQuad
  {
    FSQuad() : m_pipeline(nullptr), m_layout(nullptr),  m_renderPass(nullptr), m_fbTarget(nullptr),
               m_dlayout(nullptr) {}

    virtual ~FSQuad();

    void Create(VkDevice a_device, const char* a_vspath, const char* a_fspath, RenderTargetInfo2D a_rtInfo);
    void SetRenderTarget(VkImageView a_imageView, int a_width, int a_height); 

    void DrawCmd(VkCommandBuffer a_cmdBuff, VkDescriptorSet a_textDSet, float* a_offsAndScale = nullptr);

  protected:

    FSQuad(const FSQuad& a_rhs) { }
    FSQuad& operator=(const FSQuad& a_rhs) { return *this; }

    VkDevice         m_device;
    VkPipeline       m_pipeline;
    VkPipelineLayout m_layout;
    VkRenderPass     m_renderPass;
    VkExtent2D       m_fbSize;

    VkFramebuffer    m_fbTarget;
    VkImageView      m_targetView;
    
    // this is for binding texture
    //
    VkDescriptorSetLayout m_dlayout;

  };

  

};

#endif
