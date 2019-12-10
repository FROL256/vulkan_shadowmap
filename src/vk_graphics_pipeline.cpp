#include "vk_graphics_pipeline.h"

#include <cstring>

vk_utils::GraphicsPipelineCreateInfo::GraphicsPipelineCreateInfo() 
{ 
  memset(this, 0, sizeof(GraphicsPipelineCreateInfo)); ///<! ASSUME THIS IS PLAIN OLD DATA!!!!
} 

void vk_utils::GraphicsPipelineCreateInfo::Shaders_VSFS(VkDevice a_device, const char* vs_path, const char* ps_path)
{
  auto vertShaderCode = vk_utils::ReadFile(vs_path);
  auto fragShaderCode = vk_utils::ReadFile(ps_path);

  shaderModules[0] = vk_utils::CreateShaderModule(a_device, vertShaderCode);
  shaderModules[1] = vk_utils::CreateShaderModule(a_device, fragShaderCode);

  shaderStageInfo[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageInfo[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStageInfo[0].module = shaderModules[0];
  shaderStageInfo[0].pName  = "main";

  shaderStageInfo[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageInfo[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStageInfo[1].module = shaderModules[1];
  shaderStageInfo[1].pName  = "main";

  m_stagesNum = 2;
}

void vk_utils::GraphicsPipelineCreateInfo::Shaders_VS(VkDevice a_device, const char* vs_path)
{
  auto vertShaderCode = vk_utils::ReadFile(vs_path);

  shaderModules[0] = vk_utils::CreateShaderModule(a_device, vertShaderCode);

  shaderStageInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStageInfo[0].module = shaderModules[0];
  shaderStageInfo[0].pName = "main";

  m_stagesNum = 1;
}

VkPipelineInputAssemblyStateCreateInfo vk_utils::DefaultInputAssemblyTList()
{
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;
  return inputAssembly;
}

VkPipelineLayout vk_utils::GraphicsPipelineCreateInfo::Layout_Simple3D_VSFS(VkDevice a_device, uint32_t a_width, uint32_t a_height, VkDescriptorSetLayout a_dslayout, 
                                                                            uint32_t a_pcRangeSize, VkPipelineInputAssemblyStateCreateInfo a_inputAssembly)
{
  auto m_device = a_device;
  VkExtent2D a_screenExtent{ uint32_t(a_width), uint32_t(a_height) };

  viewport.x        = 0.0f;
  viewport.y        = 0.0f;
  viewport.width    = (float)a_screenExtent.width;
  viewport.height   = (float)a_screenExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  scissor.offset   = { 0, 0 };
  scissor.extent   = a_screenExtent;

  //
  //
  inputAssembly    = a_inputAssembly;

  viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports    = &viewport;
  viewportState.scissorCount  = 1;
  viewportState.pScissors     = &scissor;

  rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable        = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode             = VK_POLYGON_MODE_FILL; // VK_POLYGON_MODE_FILL; // VK_POLYGON_MODE_LINE
  rasterizer.lineWidth               = 1.0f;
  rasterizer.cullMode                = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable         = VK_FALSE;

  multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable  = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable    = VK_FALSE;

  colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable     = VK_FALSE;
  colorBlending.logicOp           = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount   = 1;
  colorBlending.pAttachments      = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  pcRange.stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  pcRange.offset     = 0;
  pcRange.size       = a_pcRangeSize; 

  pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges    = &pcRange;
  pipelineLayoutInfo.pSetLayouts            = &a_dslayout; //&descriptorSetLayout;
  pipelineLayoutInfo.setLayoutCount         = 1;

  if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    throw std::runtime_error("[GraphicsPipelineCreateInfo::Layout_Simple3D_VSFS]: failed to create pipeline layout!");

  depthStencilTest.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilTest.depthTestEnable       = true;
  depthStencilTest.depthWriteEnable      = true;
  depthStencilTest.depthCompareOp        = VK_COMPARE_OP_LESS;
  depthStencilTest.depthBoundsTestEnable = false;
  depthStencilTest.stencilTestEnable     = false;
  depthStencilTest.depthBoundsTestEnable = VK_FALSE;
  depthStencilTest.minDepthBounds        = 0.0f; // Optional
  depthStencilTest.maxDepthBounds        = 1.0f; // Optional

  return m_pipelineLayout;
}


VkPipeline vk_utils::GraphicsPipelineCreateInfo::Pipeline(VkDevice a_device, VkPipelineVertexInputStateCreateInfo a_vertexLayout, VkRenderPass a_renderPass)
{
  auto m_device        = a_device;
  auto vertexInputInfo = a_vertexLayout;

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.flags               = 0; 
  pipelineInfo.stageCount          = m_stagesNum;
  pipelineInfo.pStages             = shaderStageInfo;
  pipelineInfo.pVertexInputState   = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState      = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState   = &multisampling;
  pipelineInfo.pColorBlendState    = &colorBlending;
  pipelineInfo.layout              = m_pipelineLayout;
  pipelineInfo.renderPass          = a_renderPass;
  pipelineInfo.subpass             = 0;
  pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
  pipelineInfo.pDepthStencilState  = &depthStencilTest;

  if (vkCreateGraphicsPipelines(a_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
    throw std::runtime_error("[GraphicsPipelineCreateInfo::Pipeline]: failed to create graphics pipeline!");

  // free all shader modules due to we don't need them anymore
  //
  for (int i = 0; i < SHADERS_STAGE; i++)
  {
    if(shaderModules[i] != VK_NULL_HANDLE)
      vkDestroyShaderModule(a_device, shaderModules[i], VK_NULL_HANDLE);
    shaderModules[i] = VK_NULL_HANDLE;
  }

  return m_pipeline;
}
