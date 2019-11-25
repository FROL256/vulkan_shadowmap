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

#include "vk_utils.h" // #TODO: remome this!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

namespace vk_geom
{

  struct VulkanContext
  {
    VkDevice         dev           = nullptr;
    VkPhysicalDevice physDev       = nullptr;
    VkQueue          transferQueue = nullptr;
    VkQueue          computeQueue  = nullptr;
  };

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // GPU stored mesh interface
  //
  struct IMesh
  {
    IMesh(){}
    virtual ~IMesh(){}
    
    /**
    * \brief set vulkan objects that will be used by the implementation
    */
    virtual void SetVulkanContext(VulkanContext a_context) = 0;
    
    /**
    * \brief Creates internal buffer objects but DO NOT allocate memory and do not bind buffers to a memory location
    * \param a_vertNum     - input vertices number
    * \param a_indexNum    - input indices number
    *
    *        This function is actually duplicates MemoryAmount functionality
    */
    virtual VkMemoryRequirements CreateBuffers(int a_vertNum, int a_indexNum) = 0;

    /**
    * \brief Bind mesh vertex and index buffers to the memory location
    * \param a_memLoc   - input memory storage for mesh (must be allocated by the application level itself)
    
    *        This function binds internal buffer objects to a device memory location (a_memStorage + a_offset) of MemoryAmount(...) size.
    *        If internal buffers are already bound to some memory location, they will be rebound in the case when input parameters are differs to previous call.
    *        The implementation must remember parameters (but it does not own the storage, the application does).
    */
    virtual void BindBuffers(VkDeviceMemory a_memStorage, size_t a_offset) = 0;

   /**
    * \brief Updates GPU data (i.e. from CPU to GPU).
    * \param a_mesh - input simple mesh
 
    */
    virtual void                                 Update(const cmesh::SimpleMesh& a_mesh, vk_utils::ICopyEngine* a_pCopyEngine) = 0;
    
    virtual std::vector<VkBuffer>                VertexBuffers()   = 0;
    virtual VkBuffer                             IndexBuffer()     = 0;
    virtual VkPipelineVertexInputStateCreateInfo VertexInputInfo() = 0;
    
    virtual size_t      VerticesNum() const  = 0;
    virtual size_t      IndicesNum()  const  = 0;
    virtual VkIndexType IndexType()   const { return VK_INDEX_TYPE_UINT32; }

  protected:

    IMesh(const IMesh& a_rhs) = delete;
    IMesh& operator=(const IMesh& a_rhs) = delete;

    struct MemoryLocation
    {
      MemoryLocation() : memStorage(nullptr), offsetInStorage(0) { }
      MemoryLocation(VkDeviceMemory a_mem, size_t a_offs) : memStorage(a_mem), offsetInStorage(a_offs) {}
  
      bool operator==(const MemoryLocation& rhs) const { return (memStorage == rhs.memStorage) && (offsetInStorage == rhs.offsetInStorage); }
      bool operator!=(const MemoryLocation& rhs) const { return (memStorage != rhs.memStorage) || (offsetInStorage != rhs.offsetInStorage); }
      bool IsEmpty()                             const { return (memStorage == nullptr); }
  
      VkDeviceMemory   memStorage;
      size_t           offsetInStorage;
    };

  };
  
  // pack all vertex attributes in 2 float4, store them in 2 buffers. 
  // float4(0): float3 pos; uint normCompressed; 
  // float4(1): float2 texCoord; uint tangentCompressed; float reserve; 
  //
  struct CompactMesh_T3V4x2F : public IMesh
  {
    CompactMesh_T3V4x2F();
    ~CompactMesh_T3V4x2F();

    void                                 SetVulkanContext(VulkanContext a_context)                           override;

    VkMemoryRequirements                 CreateBuffers(int a_vertNum, int a_indexNum)                        override;
    void                                 BindBuffers(VkDeviceMemory a_memStorage, size_t a_offset)           override;

    void                                 Update(const cmesh::SimpleMesh& a_mesh, vk_utils::ICopyEngine* a_pCopyEngine) override;
    
    std::vector<VkBuffer>                VertexBuffers()   override;
    VkBuffer                             IndexBuffer()     override;
    VkPipelineVertexInputStateCreateInfo VertexInputInfo() override;

    size_t                               VerticesNum() const  override { return size_t(m_vertNum); };
    size_t                               IndicesNum()  const  override { return size_t(m_indNum); };

  protected:

    void DestroyBuffersIfNeeded();

    VkBuffer         m_vertexBuffers[2];
    VkBuffer         m_indexBuffer;
    MemoryLocation   m_memStorage;
    VkPhysicalDevice m_physDev;
    VkDevice         m_dev;
    VkQueue          m_transferQueue;

    int m_vertNum, m_indNum;

    // temporary data
    //
    VkVertexInputBindingDescription   vInputBindings[2] = {};
    VkVertexInputAttributeDescription vAttributes[2]    = {};
    size_t                            buffOffsets[3]    = {};

  };


};



#endif // VULKAN_GEOM_H
