#include "vk_program.h"

void vk_utils::ProgramBindings::BindBegin(VkShaderStageFlagBits a_shaderStage)
{
  m_stageFlags = a_shaderStage;
  
}

void vk_utils::ProgramBindings::BindBuffer(uint32_t a_loc, VkBufferView a_buffView)
{

}

void vk_utils::ProgramBindings::BindImage(uint32_t a_loc, VkBufferView a_buffView)
{

}

void vk_utils::ProgramBindings::BindEnd()
{

}

