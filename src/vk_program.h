#ifndef VULKAN_PROGRAM_H
#define VULKAN_PROGRAM_H

#include <vulkan/vulkan.h>
#include "vk_utils.h"



namespace vk_utils
{ 

  struct ProgramBindings
  {
    ProgramBindings(){}
    virtual ~ProgramBindings();
  
    virtual void BindBegin (VkShaderStageFlagBits a_shaderStage);
    virtual void BindBuffer(uint32_t a_loc, VkBufferView a_buffView);
    virtual void BindImage (uint32_t a_loc, VkBufferView a_buffView);
    virtual void BindEnd   ();

    virtual VkDescriptorSetLayout DLayout() const { return m_currLayout; }
    virtual VkDescriptorSet       DSet()    const { return m_currSet;    }
   
  protected:

    ProgramBindings(const ProgramBindings& a_rhs){}
    ProgramBindings& operator=(const ProgramBindings& a_rhs) { return *this; }

    VkDescriptorSetLayout m_currLayout;
    VkDescriptorSet       m_currSet;
    VkShaderStageFlagBits m_stageFlags;
    
  };

};

#endif