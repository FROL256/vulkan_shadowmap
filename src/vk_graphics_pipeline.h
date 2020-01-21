#pragma once

#include <map>
#include <unordered_map>
#include "vk_utils.h"

namespace vk_utils
{
  VkPipelineInputAssemblyStateCreateInfo IA_TList();
  VkPipelineInputAssemblyStateCreateInfo IA_PList();
  VkPipelineInputAssemblyStateCreateInfo IA_LList();
  VkPipelineInputAssemblyStateCreateInfo IA_LSList();

  struct GraphicsPipelineBuilder
  {
    enum {MAXSHADERS = 10};

    typedef typename std::unordered_map<VkShaderStageFlagBits, std::string> ShaderList;

    VkShaderModule                  shaderModules[MAXSHADERS];
    VkPipelineShaderStageCreateInfo shaderStageInfos[MAXSHADERS];

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

    GraphicsPipelineBuilder();

    void Shaders(VkDevice a_device, const std::unordered_map<VkShaderStageFlagBits, std::string> &shader_paths);

    VkPipelineLayout Layout(VkDevice a_device, VkDescriptorSetLayout a_dslayout, uint32_t a_pcRangeSize);

    void             DefaultState_Simple3D(uint32_t a_width, uint32_t a_height);

    VkPipeline       Pipeline(VkDevice a_device, VkPipelineVertexInputStateCreateInfo a_vertexLayout, VkRenderPass a_renderPass,
                                  VkPipelineInputAssemblyStateCreateInfo a_inputAssembly = IA_TList());

  protected:

    VkPipelineLayout m_pipelineLayout;
    VkPipeline       m_pipeline;
    int              m_stagesNum;


  };
}
