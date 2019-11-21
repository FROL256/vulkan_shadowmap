//
// Created by frol on 15.06.19.
//

#ifndef VULKAN_GEOM_H
#define VULKAN_GEOM_H

#include <vulkan/vulkan.h>
#include <vector>

#include <stdexcept>
#include <sstream>
#include <memory>

namespace vk_geom
{
  // very simple utility mesh representation for working with geometry on the CPU in C++
  //
  struct SimpleMeshData
  {

  };

  SimpleMeshData LoadMeshFromVSGF(const char* a_fileName);

  // GPU stored mesh interface
  //
  struct IMesh
  {
    IMesh(){}
    virtual ~IMesh(){}

    virtual bool                                 LoadFromFile(const char* a_fileName) = 0;
    virtual std::vector<VkBuffer>                VertexBuffers()   = 0;
    virtual VkPipelineVertexInputStateCreateInfo VertexInputInfo() = 0;

  protected:
    IMesh(const IMesh& a_rhs) = delete;
    IMesh& operator=(const IMesh& a_rhs) = delete;
  };
  
  // pack all vertex attributes in 2 float4, store them in 2 buffers. 
  // float4(0): float3 pos; uint normCompressed; 
  // float4(1): float2 texCoord; uint tangeCompressed; float reserve; 
  //
  struct SimpleCompactMesh : public IMesh
  {
    SimpleCompactMesh();
    ~SimpleCompactMesh();
  };

  bool                                 LoadFromFile(const char* a_fileName);
  std::vector<VkBuffer>                VertexBuffers();
  VkPipelineVertexInputStateCreateInfo VertexInputInfo();




  std::shared_ptr<SimpleCompactMesh> CreateQuadMesh(int a_sizeX, int a_sizeY, float a_size);

};

#endif // VULKAN_GEOM_H
