//////////////////////////////////
// Created by frol on 15.06.19. //
//////////////////////////////////

#ifndef VULKAN_GEOM_H
#define VULKAN_GEOM_H

#include <vulkan/vulkan.h>
#include <vector>

#include <stdexcept>
#include <sstream>
#include <memory>

#include "cmesh.h"

namespace vk_geom
{
  struct MemoryLocation
  {
    MemoryLocation() : dev(nullptr), memStorage(nullptr), offsetInStorage(0) { }
    MemoryLocation(VkDevice a_dev, VkDeviceMemory a_mem, size_t a_offs) : dev(a_dev), memStorage(a_mem), offsetInStorage(a_offs) {}

    bool operator==(const MemoryLocation& rhs) const { return (dev == rhs.dev) && (memStorage == rhs.memStorage) && (offsetInStorage == rhs.offsetInStorage); }
    bool operator!=(const MemoryLocation& rhs) const { return (dev != rhs.dev) || (memStorage != rhs.memStorage) || (offsetInStorage != rhs.offsetInStorage); }
    bool IsEmpty()                             const { return *(this) != MemoryLocation(); }

    VkDevice         dev;
    VkDeviceMemory   memStorage;
    size_t           offsetInStorage;
  };

  // GPU stored mesh interface
  //
  struct IMesh
  {
    IMesh(){}
    virtual ~IMesh(){}
    
    /**
    * \brief Tells us the amount of device memory that needed for the mesh. 
    * \param a_vertNum     - input vertices number
    * \param a_indexNum    - input indices number
    * 
    *        An application level should use this function to determine the correct amount of memory which application level must allocate itself before it decide to update mesh
    */
    virtual size_t                               MemoryAmount(int a_vertNum, int a_indexNum) = 0;

     /**
    * \brief Updates GPU data (i.e. from CPU to GPU).
    * \param a_mesh        - input simple mesh
    * \param a_memLoc      - input memory storage for mesh (must be allocated by the application level itself)
    
    *        This function binds internal buffer objects to a device memory location (a_memLoc.memStorage + a_memLoc.offsetInStorage) of MemoryAmount(...) size.
    *        If internal buffers are already bound to some memory location, they will be rebound in the case when a_memLoc data members are differs to previous call.
    *        The implementation must remember a_memLoc parameters (but it does not own the storage, the application does).
    *        If the application would like to just update data, it can omit a_memLoc parametrer. In this case previously remembered storage will be used.
    */
    virtual void                                 UpdateAndBind(const cmesh::SimpleMesh& a_mesh, MemoryLocation a_memLoc = MemoryLocation()) = 0;
    virtual std::vector<VkBuffer>                VertexBuffers()   = 0;
    virtual VkBuffer                             IndexBuffer()     = 0;
    virtual VkPipelineVertexInputStateCreateInfo VertexInputInfo() = 0;

  protected:
    IMesh(const IMesh& a_rhs) = delete;
    IMesh& operator=(const IMesh& a_rhs) = delete;
  };
  
  // pack all vertex attributes in 2 float4, store them in 2 buffers. 
  // float4(0): float3 pos; uint normCompressed; 
  // float4(1): float2 texCoord; uint tangentCompressed; float reserve; 
  //
  struct CompactMesh_T3V4x2F : public IMesh
  {
    CompactMesh_T3V4x2F();
    ~CompactMesh_T3V4x2F();

    size_t                               MemoryAmount(int a_vertNum, int a_indexNum);
    void                                 UpdateAndBind(const cmesh::SimpleMesh& a_mesh, MemoryLocation a_loc);
    std::vector<VkBuffer>                VertexBuffers();
    VkBuffer                             IndexBuffer();
    VkPipelineVertexInputStateCreateInfo VertexInputInfo();

  protected:

   void DestroyBuffersIfNeeded();

   VkBuffer       m_vertexBuffers[2];
   VkBuffer       m_indexBuffer;
   MemoryLocation m_memStorage;

  };


};



#endif // VULKAN_GEOM_H
