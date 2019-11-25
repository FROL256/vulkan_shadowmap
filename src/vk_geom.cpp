#include "vk_geom.h"
#include "vk_utils.h"

#include <cstring>
#include <stdexcept>

static inline unsigned int EncodeNormal(const float n[3])
{
  const int x = (int)(n[0]*32767.0f);
  const int y = (int)(n[1]*32767.0f);

  const unsigned int sign = (n[2] >= 0) ? 0 : 1;
  const unsigned int sx   = ((unsigned int)(x & 0xfffe) | sign);
  const unsigned int sy   = ((unsigned int)(y & 0xffff) << 16);

  return (sx | sy);
}

//static inline float3 decodeNormal(unsigned int a_data)
//{  
//  const unsigned int a_enc_x = (a_data  & 0x0000FFFF);
//  const unsigned int a_enc_y = ((a_data & 0xFFFF0000) >> 16);
//  const float sign           = (a_enc_x & 0x0001) ? -1.0f : 1.0f;
//
//  const float x = ((short)(a_enc_x & 0xfffe))*(1.0f / 32767.0f);
//  const float y = ((short)(a_enc_y & 0xffff))*(1.0f / 32767.0f);
//  const float z = sign*sqrt(fmax(1.0f - x*x - y*y, 0.0f));
//
//  return float3(x, y, z);
//}

static inline unsigned int as_uint(float x)
{ 
  unsigned int res;
  memcpy(&res, &x, sizeof(unsigned int)); // this is fine, try godbolt compiler explorer!
  return res;
}

static inline float as_float(unsigned int x) 
{ 
  float res;
  memcpy(&res, &x, sizeof(float)); // this is fine, try godbolt compiler explorer!
  return res;
}

vk_geom::CompactMesh_T3V4x2F::CompactMesh_T3V4x2F()
{
   m_vertexBuffers[0] = nullptr;
   m_vertexBuffers[1] = nullptr;
   m_indexBuffer      = nullptr;   
}

vk_geom::CompactMesh_T3V4x2F::~CompactMesh_T3V4x2F()
{
  DestroyBuffersIfNeeded();
}

void vk_geom::CompactMesh_T3V4x2F::SetVulkanContext(VulkanContext a_context)
{
  m_dev            = a_context.dev;
  m_physDev        = a_context.physDev;
  m_transferQueue  = a_context.transferQueue;
}

void vk_geom::CompactMesh_T3V4x2F::DestroyBuffersIfNeeded()
{
  if(m_vertexBuffers[0] != nullptr && m_indexBuffer != nullptr)
  {
    vkDestroyBuffer(m_dev, m_vertexBuffers[0], NULL);
    vkDestroyBuffer(m_dev, m_vertexBuffers[1], NULL);
    vkDestroyBuffer(m_dev, m_indexBuffer, NULL);
  }
}

size_t Padding(size_t a_size, size_t a_aligment)
{
  if(a_size % a_aligment == 0)
    return a_size;
  else
  {
    size_t sizeCut = a_size - (a_size % a_aligment);
    return sizeCut + a_aligment;
  }
}

VkMemoryRequirements vk_geom::CompactMesh_T3V4x2F::CreateBuffers(int a_vertNum, int a_indexNum)
{
  assert(m_dev != nullptr); // you should set Vulkan context before using this function

  DestroyBuffersIfNeeded();

  const size_t f4size = a_vertNum * sizeof(float) * 4;

  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size  = f4size;
  // this class assume GPU simulation for mesh, i.e. we want to write buffer in the compute shader 
  // we also want to transfer data there .. 
  bufferCreateInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VK_CHECK_RESULT(vkCreateBuffer(m_dev, &bufferCreateInfo, NULL, &m_vertexBuffers[0]));
  VK_CHECK_RESULT(vkCreateBuffer(m_dev, &bufferCreateInfo, NULL, &m_vertexBuffers[1]));

  bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferCreateInfo.size  = a_indexNum*sizeof(int);
  VK_CHECK_RESULT(vkCreateBuffer(m_dev, &bufferCreateInfo, NULL, &m_indexBuffer));
  
  m_vertNum = a_vertNum;
  m_indNum  = a_indexNum;

  // piece of shit with memory aligment
  //
  VkMemoryRequirements memoryRequirements[3];
  vkGetBufferMemoryRequirements(m_dev, m_vertexBuffers[0], &memoryRequirements[0]);
  vkGetBufferMemoryRequirements(m_dev, m_vertexBuffers[1], &memoryRequirements[1]);
  vkGetBufferMemoryRequirements(m_dev, m_indexBuffer,      &memoryRequirements[2]);

  buffOffsets[0] = 0;
  buffOffsets[1] = Padding(memoryRequirements[0].size                             , memoryRequirements[1].alignment);
  buffOffsets[2] = Padding(memoryRequirements[0].size + memoryRequirements[1].size, memoryRequirements[2].alignment);

  memoryRequirements[0].size = buffOffsets[2] + memoryRequirements[2].size;

  assert(memoryRequirements[0].alignment == memoryRequirements[1].alignment);
  assert(memoryRequirements[0].alignment == memoryRequirements[2].alignment);

  assert(memoryRequirements[0].memoryTypeBits == memoryRequirements[1].memoryTypeBits);
  assert(memoryRequirements[0].memoryTypeBits == memoryRequirements[2].memoryTypeBits);

  return memoryRequirements[0];
}

void vk_geom::CompactMesh_T3V4x2F::BindBuffers(VkDeviceMemory a_memStorage, size_t a_offset)
{
  assert(m_dev != nullptr); // you should set Vulkan context before using this function

  auto a_loc = MemoryLocation{a_memStorage, a_offset};

  if((m_memStorage != a_loc) && !a_loc.IsEmpty()) // rebound is needed
  {
    m_memStorage.memStorage      = a_memStorage;
    m_memStorage.offsetInStorage = a_offset;
   
    VK_CHECK_RESULT(vkBindBufferMemory(m_dev, m_vertexBuffers[0], m_memStorage.memStorage, buffOffsets[0] ));
    VK_CHECK_RESULT(vkBindBufferMemory(m_dev, m_vertexBuffers[1], m_memStorage.memStorage, buffOffsets[1] ));
    VK_CHECK_RESULT(vkBindBufferMemory(m_dev, m_indexBuffer,      m_memStorage.memStorage, buffOffsets[2] ));
  }

  if(m_memStorage.IsEmpty())
    RUN_TIME_ERROR("[CompactMesh_T3V4x2F::BindBuffers()]: empty input and/or internal storage!");
}

void vk_geom::CompactMesh_T3V4x2F::Update(const cmesh::SimpleMesh& a_mesh, vk_utils::ICopyEngine* a_pCopyEngine)
{
  assert(a_mesh.VerticesNum() == m_vertNum);
  assert(a_mesh.IndicesNum()  == m_indNum);
  assert(a_pCopyEngine        != nullptr);

  std::vector<float> vPosNorm4f        (a_mesh.VerticesNum()*4);
  std::vector<float> vTexCoordAndTang4f(a_mesh.VerticesNum()*4);

  for(int i=0;i<a_mesh.VerticesNum();i++)
  {
    vPosNorm4f[i*4+0] = a_mesh.vPos4f[i*4+0];
    vPosNorm4f[i*4+1] = a_mesh.vPos4f[i*4+1];
    vPosNorm4f[i*4+2] = a_mesh.vPos4f[i*4+2];
    vPosNorm4f[i*4+3] = as_float(EncodeNormal(a_mesh.vNorm4f.data() + i*4));    

    vTexCoordAndTang4f[i*4+0] = a_mesh.vTexCoord2f[i*2+0];
    vTexCoordAndTang4f[i*4+1] = a_mesh.vTexCoord2f[i*2+1];
    vTexCoordAndTang4f[i*4+2] = as_float(EncodeNormal(a_mesh.vTang4f.data() + i*4));
    vTexCoordAndTang4f[i*4+3] = 0.0f; // reserved
  }

  a_pCopyEngine->UpdateBuffer(m_vertexBuffers[0], 0, vPosNorm4f.data(),         sizeof(float)*vPosNorm4f.size());
  a_pCopyEngine->UpdateBuffer(m_vertexBuffers[1], 0, vTexCoordAndTang4f.data(), sizeof(float)*vTexCoordAndTang4f.size());
  a_pCopyEngine->UpdateBuffer(m_indexBuffer,      0, a_mesh.indices.data(),     sizeof(int)*a_mesh.indices.size());
}

std::vector<VkBuffer> vk_geom::CompactMesh_T3V4x2F::VertexBuffers()
{
  return std::vector<VkBuffer>(m_vertexBuffers, m_vertexBuffers + 2);
}

VkBuffer vk_geom::CompactMesh_T3V4x2F::IndexBuffer()
{
  return m_indexBuffer;
}

VkPipelineVertexInputStateCreateInfo vk_geom::CompactMesh_T3V4x2F::VertexInputInfo()
{
  vInputBindings[0].binding   = 0;
  vInputBindings[0].stride    = sizeof(float) * 4;
  vInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  vInputBindings[1].binding   = 1;
  vInputBindings[1].stride    = sizeof(float) * 4;
  vInputBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  vAttributes[0].binding  = 0;
  vAttributes[0].location = 0;
  vAttributes[0].format   = VK_FORMAT_R32G32B32A32_SFLOAT; 
  vAttributes[0].offset   = 0;   
  
  vAttributes[1].binding  = 1;
  vAttributes[1].location = 1;
  vAttributes[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
  vAttributes[1].offset   = 0;  
  
  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount   = 2;
  vertexInputInfo.vertexAttributeDescriptionCount = 2;
  vertexInputInfo.pVertexBindingDescriptions      = vInputBindings;
  vertexInputInfo.pVertexAttributeDescriptions    = vAttributes;

  return vertexInputInfo;   
}