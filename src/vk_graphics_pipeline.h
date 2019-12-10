#pragma once
#include "vk_utils.h"

namespace vk_utils
{

  VkPipelineInputAssemblyStateCreateInfo DefaultInputAssemblyTList();

  struct GraphicsPipelineCreateInfo
  {
    static constexpr int SHADERS_STAGE = 5;

    VkShaderModule                         shaderModules  [SHADERS_STAGE];
    VkPipelineShaderStageCreateInfo        shaderStageInfo[SHADERS_STAGE];
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;                  // set from input!
    VkViewport                             viewport;
    VkRect2D                               scissor;
    VkPipelineViewportStateCreateInfo      viewportState;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineMultisampleStateCreateInfo   multisampling;
    VkPipelineColorBlendAttachmentState    colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo    colorBlending;
    VkPushConstantRange                    pcRange;
    VkPipelineLayoutCreateInfo             pipelineLayoutInfo;
    VkPipelineDepthStencilStateCreateInfo  depthStencilTest;
    VkGraphicsPipelineCreateInfo           pipelineInfo;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    GraphicsPipelineCreateInfo() { memset(this, 0, sizeof(GraphicsPipelineCreateInfo)); } ///<! ASSUME THIS IS PLAIN OLD DATA!!!!

    void             Shaders_VS  (VkDevice a_device, const char* vs_path);
    void             Shaders_VSFS(VkDevice a_device, const char* vs_path, const char* ps_path);

    VkPipelineLayout Layout_Simple3D_VSFS(VkDevice a_device, uint32_t a_width, uint32_t a_height, VkDescriptorSetLayout a_dslayout, uint32_t a_pcRangeSize, 
                                          VkPipelineInputAssemblyStateCreateInfo a_inputAssembly = DefaultInputAssemblyTList());

    VkPipeline       Pipeline(VkDevice a_device, VkPipelineVertexInputStateCreateInfo a_vertexLayout, VkRenderPass a_renderPass);

  protected:

    VkPipelineLayout m_pipelineLayout;
    VkPipeline       m_pipeline;
    int              m_stagesNum;


  };
}
