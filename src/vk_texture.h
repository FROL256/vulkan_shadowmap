#ifndef VULKAN_TEXTURE_HELPER_H
#define VULKAN_TEXTURE_HELPER_H

#include <vulkan/vulkan.h>
#include <vector>

#include <stdexcept>
#include <sstream>
#include <memory>

namespace vk_texture
{
  // Application should implement this interface // #TODO: pass layout and curr stage explicitly for copy engine!
  //
  struct ICopyEngine
  {
    ICopyEngine(){}
    virtual ~ICopyEngine(){}

    virtual void UpdateImage(VkImage a_image, const void* a_src, int a_width, int a_height, int a_bpp) = 0; // this function assume texture is in undefined layout and it must leave texture in transfer dst layout

  protected:
    ICopyEngine(const ICopyEngine& rhs) {}
    ICopyEngine& operator=(const ICopyEngine& rhs) { return *this; }    
  };


  struct SimpleTexture2D
  {
    SimpleTexture2D() : memStorage(0), imageGPU(0), imageSampler(0), imageView(0) {}
    ~SimpleTexture2D();
   
    //// useful functions
    //
    VkMemoryRequirements CreateImage(VkDevice a_device, const int a_width, const int a_height, VkFormat a_format);
    void                 BindMemory (VkDeviceMemory a_memStorage, size_t a_offset);
    void                 Update     (const void* a_src, int a_width, int a_height, int a_bpp, ICopyEngine* a_pCopyImpl);
  
    void                 GenerateMipsCmd(VkCommandBuffer a_cmdBuff);
    void                 ChangeLayoutCmd(VkCommandBuffer a_cmdBuff, VkImageLayout a_newLayout, VkPipelineStageFlags a_newStage);
    
    //// information functions
    //
    VkImage              Image()      const { return imageGPU;     }
    VkImageView          View()       const { return imageView;    }
    VkSampler            Sampler()    const { return imageSampler; }

    VkImageCreateInfo    Info()       const { return m_createImageInfo; }
    int                  Width()      const { return m_width;  }
    int                  Height()     const { return m_height; }
    VkFormat             Format()     const { return m_format; }

    VkImageLayout        Layout()     const { return m_currentLayout; }
    VkPipelineStageFlags Stage()      const { return m_currentStage;  }

  protected:

    VkDeviceMemory memStorage; // SimpleVulkanTexture DOES NOT OWN memStorage! It just save reference to it.
    VkImage        imageGPU;
    VkSampler      imageSampler;
    VkImageView    imageView;
    VkDevice       m_device;
    VkFormat       m_format;
    int m_width, m_height;
    int m_mipLevels;
    
    VkImageCreateInfo  m_createImageInfo;

    VkImageLayout        m_currentLayout;
    VkPipelineStageFlags m_currentStage;

  };


  

};

#endif