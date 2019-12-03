#ifndef VULKAN_FSQUAD_H
#define VULKAN_FSQUAD_H

#include <vulkan/vulkan.h>

// full screen quad

namespace vk_utils
{
  struct FSQuad
  {
    FSQuad() : m_pipeline(nullptr), m_layout(nullptr), m_renderPass(nullptr), m_fbTarget(nullptr) {}
    virtual ~FSQuad();

    void Create(VkDevice a_device, VkExtent2D a_fbRes, VkFormat a_fbFormat, const char* a_vspath, const char* a_fspath);
    void AssignRenderTarget(VkImageView a_imageView, int a_width, int a_height); 

    void DrawCmd(VkCommandBuffer a_cmdBuff, float* a_offsAndScale = nullptr);

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
  };

  

};

#endif
