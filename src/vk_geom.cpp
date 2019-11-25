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
  m_memStorage.dev = a_context.dev;
  m_physDev        = a_context.physDev;
  m_transferQueue  = a_context.transferQueue;
}

size_t vk_geom::CompactMesh_T3V4x2F::MemoryAmount(int a_vertNum, int a_indexNum)
{
  return a_vertNum*sizeof(float)*8 + sizeof(int)*a_indexNum;
}


void vk_geom::CompactMesh_T3V4x2F::DestroyBuffersIfNeeded()
{
  if(m_vertexBuffers[0] != nullptr && m_indexBuffer != nullptr)
  {
    vkDestroyBuffer(m_memStorage.dev, m_vertexBuffers[0], NULL);
    vkDestroyBuffer(m_memStorage.dev, m_vertexBuffers[1], NULL);
    vkDestroyBuffer(m_memStorage.dev, m_indexBuffer, NULL);
  }
}

VkMemoryRequirements vk_geom::CompactMesh_T3V4x2F::CreateBuffers(int a_vertNum, int a_indexNum)
{
  assert(m_memStorage.dev != nullptr); // you should set Vulkan context before using this function

  DestroyBuffersIfNeeded();

  const size_t f4size = a_vertNum * sizeof(float) * 4;

  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = f4size;
  // this class assume GPU simulation for mesh, i.e. we want to write buffer in the compute shader 
  // we also want to transfer data there .. 
  bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VK_CHECK_RESULT(vkCreateBuffer(m_memStorage.dev, &bufferCreateInfo, NULL, &m_vertexBuffers[0]));
  VK_CHECK_RESULT(vkCreateBuffer(m_memStorage.dev, &bufferCreateInfo, NULL, &m_vertexBuffers[1]));

  bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  bufferCreateInfo.size = a_indexNum;
  VK_CHECK_RESULT(vkCreateBuffer(m_memStorage.dev, &bufferCreateInfo, NULL, &m_indexBuffer));

  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(m_memStorage.dev, m_vertexBuffers[0], &memoryRequirements);

  return memoryRequirements;
}

void vk_geom::CompactMesh_T3V4x2F::BindBuffers(int a_vertNum, int a_indexNum, MemoryLocation a_loc)
{
  if((m_memStorage != a_loc) && !a_loc.IsEmpty()) // rebound is needed
  {
    const size_t f4size = a_vertNum*sizeof(float)*4;
   
    VK_CHECK_RESULT(vkBindBufferMemory(m_memStorage.dev, m_vertexBuffers[0], m_memStorage.memStorage, 0));
    VK_CHECK_RESULT(vkBindBufferMemory(m_memStorage.dev, m_vertexBuffers[1], m_memStorage.memStorage, 1*f4size));
    VK_CHECK_RESULT(vkBindBufferMemory(m_memStorage.dev, m_indexBuffer,      m_memStorage.memStorage, 2*f4size));
  }

  if(m_memStorage.IsEmpty())
    throw std::runtime_error("[CompactMesh_T3V4x2F::BindBuffers()]: empty input and/or internal storage!");
}

void vk_geom::CompactMesh_T3V4x2F::Update(const cmesh::SimpleMesh& a_mesh)
{
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
  
  // well, we need separate staging buffer for that, ups )
  // so, let user to do this
  //
  /*
  std::vector<BufferUpdateInfo> updates(3);

  updates[0].dst  = m_vertexBuffers[0];
  updates[0].src  = vPosNorm4f.data();
  updates[0].size = sizeof(float)*vPosNorm4f.size();

  updates[1].dst  = m_vertexBuffers[1];
  updates[1].src  = vTexCoordAndTang4f.data();
  updates[1].size = sizeof(float)*vTexCoordAndTang4f.size();

  updates[2].dst  = m_indexBuffer;
  updates[2].src  = a_mesh.indices.data();
  updates[2].size = sizeof(int)*a_mesh.indices.size();

  a_updateOp(updates, a_updateOpUserData);
  */


  // // now we can update the data  
  // //
  // { 
  //   const size_t size = MemoryAmount(a_mesh.VerticesNum(), a_mesh.IndicesNum());
  //   void *mappedMemory = nullptr;
  //   vkMapMemory(m_memStorage.dev, m_memStorage.memStorage, 0, size, 0, &mappedMemory);
  //   memcpy(mappedMemory + 0*sizeof(float)*vPosNorm4f.size(), vPosNorm4f.data(),         sizeof(float)*vPosNorm4f.size());
  //   memcpy(mappedMemory + 1*sizeof(float)*vPosNorm4f.size(), vTexCoordAndTang4f.data(), sizeof(float)*vTexCoordAndTang4f.size());
  //   memcpy(mappedMemory + 2*sizeof(float)*vPosNorm4f.size(), a_mesh.indices.data(),  sizeof(int)*a_mesh.indices.size());
  //   vkUnmapMemory(m_memStorage.dev, m_memStorage.memStorage);
  // }
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
  VkVertexInputBindingDescription vInputBindings[2] = { };
  vInputBindings[0].binding   = 0;
  vInputBindings[0].stride    = sizeof(float) * 4;
  vInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  vInputBindings[1].binding   = 1;
  vInputBindings[1].stride    = sizeof(float) * 4;
  vInputBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vAttributes[2] = {};
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