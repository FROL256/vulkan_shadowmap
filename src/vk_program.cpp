#include "vk_program.h"
#include "vk_utils.h"

vk_utils::ProgramBindings::ProgramBindings(VkDevice a_device, const VkDescriptorType* a_dtypes, int a_dtypesSize, int a_poolSize) : m_device(a_device)
{ 
  m_poolSize = a_poolSize;
  m_poolTypes.assign(a_dtypes, a_dtypes + a_dtypesSize);

  std::vector<VkDescriptorPoolSize> descriptorPoolSize(m_poolTypes.size());
  for (size_t i = 0; i < descriptorPoolSize.size(); i++)
  {
    descriptorPoolSize[i].type            = m_poolTypes[i];
    descriptorPoolSize[i].descriptorCount = m_poolSize;
  }
  
  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
  descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.maxSets       = m_poolSize; // we need to allocate at least 1 descriptor set
  descriptorPoolCreateInfo.poolSizeCount = uint32_t(descriptorPoolSize.size());
  descriptorPoolCreateInfo.pPoolSizes    = descriptorPoolSize.data();
  
  VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, NULL, &m_pool));
}

vk_utils::ProgramBindings::~ProgramBindings()
{
  assert(m_device != nullptr);

  for (auto& l : m_dsLayouts)
    vkDestroyDescriptorSetLayout(m_device, l.second.layout, NULL);
  vkDestroyDescriptorPool(m_device, m_pool, NULL);
}

void vk_utils::ProgramBindings::BindBegin(VkShaderStageFlagBits a_shaderStage)
{
  m_stageFlags = a_shaderStage;
  m_currBindings.clear();
}

void vk_utils::ProgramBindings::BindBuffer(uint32_t a_loc, VkBufferView a_buffView, VkDescriptorType a_bindType)
{
  PBinding bind;
  bind.buffView  = a_buffView;
  bind.imageView = nullptr;
  bind.type      = a_bindType;
  m_currBindings[a_loc] = bind;
}

void vk_utils::ProgramBindings::BindImage(uint32_t a_loc, VkImageView a_imageView, VkDescriptorType a_bindType)
{
  PBinding bind;
  bind.buffView  = nullptr;
  bind.imageView = a_imageView;
  bind.type      = a_bindType;
  m_currBindings[a_loc] = bind;
}

void  vk_utils::ProgramBindings::BindImage(uint32_t a_loc, VkImageView a_imageView, VkSampler a_sampler, VkDescriptorType a_bindType)
{
  PBinding bind;
  bind.buffView     = nullptr;
  bind.imageView    = a_imageView;
  bind.imageSampler = a_sampler;
  bind.type         = a_bindType;
  m_currBindings[a_loc] = bind;
}

vk_utils::DSetId vk_utils::ProgramBindings::BindEnd(VkDescriptorSet* a_pSet, VkDescriptorSetLayout* a_pLayout)
{
  // create DS layout key
  //
  auto currKey = vk_utils::PBKey(m_currBindings, m_stageFlags);
  auto p       = m_dsLayouts.find(currKey);

  // get DS layout
  //
  VkDescriptorSetLayout layout = nullptr;
  if (p != m_dsLayouts.end()) // if we have such ds layout fetch it from cache
  {
    layout = p->second.layout;
  }
  else                        // otherwise create 
  {
    // With the pool allocated, we can now allocate the descriptor set layout
    //
    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding(m_currBindings.size());

    for (const auto& currB : m_currBindings)
    {
      descriptorSetLayoutBinding[currB.first].binding            = uint32_t(currB.first);
      descriptorSetLayoutBinding[currB.first].descriptorType     = currB.second.type;
      descriptorSetLayoutBinding[currB.first].descriptorCount    = 1;
      descriptorSetLayoutBinding[currB.first].stageFlags         = m_stageFlags;
      descriptorSetLayoutBinding[currB.first].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = uint32_t(descriptorSetLayoutBinding.size());
    descriptorSetLayoutCreateInfo.pBindings    = descriptorSetLayoutBinding.data();

    // Create the descriptor set layout.
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, NULL, &layout));

    m_dsLayouts[currKey] = DSData(layout);
    p = m_dsLayouts.find(currKey);
  }

  assert(p != m_dsLayouts.end());

  // create descriptor set 
  //
  VkDescriptorSet ds;

  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
  descriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAllocateInfo.descriptorPool     = m_pool;   // pool to allocate from.
  descriptorSetAllocateInfo.descriptorSetCount = 1;        // allocate a descriptor set for buffer and image
  descriptorSetAllocateInfo.pSetLayouts        = &layout;

  // allocate descriptor set.
  //
  VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &ds)); //#TODO: do not allocate if you already has such descriptor set!

  p->second.sets.push_back(ds);

  // update current descriptor set, #NOTE: THE IMPLEMENTATION IS NOT FINISHED!!!! ITS PROTOTYPE!!!!
  //
  const size_t descriptorsInSet = m_currBindings.size();

  std::vector<VkDescriptorImageInfo>  descriptorImageInfo(descriptorsInSet);
  std::vector<VkDescriptorBufferInfo> descriptorBufferInfo(descriptorsInSet);
  std::vector<VkWriteDescriptorSet>   writeDescriptorSet(descriptorsInSet);

  for (auto& binding : m_currBindings)
  {
    descriptorImageInfo[binding.first]             = VkDescriptorImageInfo{};
    descriptorImageInfo[binding.first].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorImageInfo[binding.first].imageView   = binding.second.imageView;
    descriptorImageInfo[binding.first].sampler     = binding.second.imageSampler;

    descriptorBufferInfo[binding.first]        = VkDescriptorBufferInfo{};
    descriptorBufferInfo[binding.first].buffer = nullptr;          // #TODO: update here!
    descriptorBufferInfo[binding.first].offset = 0;                // #TODO: update here!
    descriptorBufferInfo[binding.first].range  = 0;                // #TODO: update here!

    writeDescriptorSet[binding.first]                 = VkWriteDescriptorSet{};
    writeDescriptorSet[binding.first].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet[binding.first].dstSet          = ds;
    writeDescriptorSet[binding.first].dstBinding      = binding.first;
    writeDescriptorSet[binding.first].descriptorCount = 1;
    writeDescriptorSet[binding.first].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // #TODO: update here!
    writeDescriptorSet[binding.first].pImageInfo      = &descriptorImageInfo[binding.first];
  }

  vkUpdateDescriptorSets(m_device, uint32_t(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, NULL);
  
  if(a_pSet != nullptr)
    (*a_pSet) = ds;

  if(a_pLayout != nullptr)
    (*a_pLayout) = layout;

  // return an id that we can use to retrieve same descriptor set later
  //
  DSetId res;
  res.key       = p->first;
  res.dSetIndex = p->second.sets.size() - 1;
  return res;
}

VkDescriptorSetLayout vk_utils::ProgramBindings::DLayout(const DSetId& a_setId) const
{
  auto p = m_dsLayouts.find(a_setId.key);
  if(p == m_dsLayouts.end())
    return nullptr;

  return p->second.layout;
}

VkDescriptorSet vk_utils::ProgramBindings::DSet(const DSetId& a_setId) const
{
  auto p = m_dsLayouts.find(a_setId.key);
  if (p == m_dsLayouts.end())
    return nullptr;

  if (a_setId.dSetIndex >= p->second.sets.size())
    return nullptr;

  return p->second.sets[a_setId.dSetIndex];
}
