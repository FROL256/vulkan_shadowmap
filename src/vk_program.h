#ifndef VULKAN_PROGRAM_H
#define VULKAN_PROGRAM_H

#include <vulkan/vulkan.h>
#include "vk_utils.h"

#include <vector>
#include <map>
#include <unordered_map>

namespace vk_utils
{
  struct PBinding
  {
    bool operator==(const PBinding& rhs) const
    {
      bool buffsMatch = ((buffView != nullptr) && (rhs.buffView != nullptr)) || 
                        ((buffView == nullptr) && (rhs.buffView == nullptr));

      bool imgMatch = ((imageView != nullptr) && (rhs.imageView != nullptr)) ||
                      ((imageView == nullptr) && (rhs.imageView == nullptr));

      bool samMatch = ((imageSampler != nullptr) && (rhs.imageSampler != nullptr)) ||
                      ((imageSampler == nullptr) && (rhs.imageSampler == nullptr));

      return buffsMatch && imgMatch && samMatch && (type == rhs.type);
    }

    VkBufferView buffView     = nullptr;
    VkImageView  imageView    = nullptr;
    VkSampler    imageSampler = nullptr;
    VkDescriptorType type;
  };


  struct PBKey
  {
    PBKey() {}
    PBKey(const std::map<int, PBinding>& a_currBindings, VkShaderStageFlags a_shaderStage) : m_currBindings(a_currBindings), m_stage(a_shaderStage) {}

    bool operator==(const PBKey& rhs) const
    {
      if (rhs.m_currBindings.size() != m_currBindings.size())
        return false;

      if (rhs.m_stage != m_stage)
        return false;

      bool equals = true;

      for (auto p : m_currBindings)
      {
        auto p2 = rhs.m_currBindings.find(p.first);
        if (p2 != rhs.m_currBindings.end() && !(p2->second == p.second))
        {
          equals = false;
          break;
        }
      }

      return equals;
    }

    std::map<int, PBinding> m_currBindings;
    VkShaderStageFlags      m_stage;
  };
};

namespace std 
{

  template <>
  struct hash<vk_utils::PBKey>
  {
    std::size_t operator()(const vk_utils::PBKey& k) const
    {
      using std::size_t;
      using std::hash;
      using std::string;

      // Compute individual hash values for first,
      // second and third and combine them using XOR and bit shifting:
      //
      size_t currHash = k.m_currBindings.size();

      for (const auto& b : k.m_currBindings)
      {
        currHash ^= ( (hash<int>()(b.first)));
        currHash ^= ( (hash<bool>()(b.second.buffView    == nullptr)   << 1 ));
        currHash ^= ( (hash<bool>()(b.second.imageView    == nullptr)  << 2 ));
        currHash ^= ( (hash<bool>()(b.second.imageSampler == nullptr)) << 3);
        currHash ^= ( (hash<int> ()(b.second.type))                    << 7);
      }

      return currHash;
    }
  };
}

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
    virtual DSetId BindEnd ();

    virtual VkDescriptorSetLayout DSetLayout(const DSetId& a_setId) const;
    virtual VkDescriptorSet       DSet      (const DSetId& a_setId) const;
   

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