#ifndef VULKAN_PROGRAM_H
#define VULKAN_PROGRAM_H

#include <vulkan/vulkan.h>
#include "vk_utils.h"

#include <vector>
#include <map>
#include <unordered_map>

#include "vk_program_helper.h"

namespace vk_utils
{ 
  struct DSetId
  {
    PBKey  key;
    size_t dSetIndex;
  };

  struct ProgramBindings
  {
    ProgramBindings(VkDevice a_device, const VkDescriptorType* a_dtypes, int a_dtypesSize, int a_poolSize = 64);
    virtual ~ProgramBindings();
  
    virtual void BindBegin (VkShaderStageFlagBits a_shaderStage);
    virtual void BindBuffer(uint32_t a_loc, VkBufferView a_buffView, VkDescriptorType a_bindType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    virtual void BindImage (uint32_t a_loc, VkImageView  a_imageView, VkDescriptorType a_bindType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    virtual void BindImage (uint32_t a_loc, VkImageView  a_imageView, VkSampler a_sampler, VkDescriptorType a_bindType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    virtual DSetId BindEnd (VkDescriptorSet* a_pSet = nullptr, VkDescriptorSetLayout* a_pLayout = nullptr);

    virtual VkDescriptorSetLayout DLayout(const DSetId& a_setId) const;
    virtual VkDescriptorSet       DSet   (const DSetId& a_setId) const;
   
  protected:

    VkDescriptorPool m_pool;
    int              m_poolSize;
    std::vector<VkDescriptorType> m_poolTypes;

    ProgramBindings(const ProgramBindings& a_rhs){}
    ProgramBindings& operator=(const ProgramBindings& a_rhs) { return *this; }

    VkDescriptorSetLayout m_currLayout;
    VkDescriptorSet       m_currSet;
    VkShaderStageFlagBits m_stageFlags;
    VkDevice              m_device;

    std::map<int, PBinding> m_currBindings;
    
    struct DSData
    {
      DSData() { sets.reserve(16); }
      DSData(VkDescriptorSetLayout a_layout) : layout(a_layout) { sets.reserve(16); }
      VkDescriptorSetLayout        layout;
      std::vector<VkDescriptorSet> sets;
    };

    std::unordered_map<PBKey, DSData> m_dsLayouts;
  };

};

#endif