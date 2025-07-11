#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace hkr {

// general image (may not be used directly)
class ImageBase {
public:
  ImageBase() = default;
  ~ImageBase() = default;
  ImageBase(VkDevice device,
            VmaAllocator allocator,
            VmaAllocatorCreateFlags allocFlags,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            uint32_t mipLevels,
            uint32_t arrayLayers,
            VkFormat format,
            VkImageUsageFlags usage,
            VkSampleCountFlagBits numSamples);
  void Create(VkDevice device,
              VmaAllocator allocator,
              VmaAllocatorCreateFlags allocFlags,
              uint32_t width,
              uint32_t height,
              uint32_t depth,
              uint32_t mipLevels,
              uint32_t arrayLayers,
              VkFormat format,
              VkImageUsageFlags usage,
              VkSampleCountFlagBits numSamples);
  void Cleanup(VkDevice device, VmaAllocator allocator);

  VkImage image;
  VkImageView imageView;
  VmaAllocation allocation;
};

// color/depth attachment, storage image with dedicated memory allocation
class Image : public ImageBase {
public:
  Image() = default;
  ~Image() = default;
  Image(VkDevice device,
        VmaAllocator allocator,
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        VkFormat format,
        VkImageUsageFlags usage,
        VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
  void Create(VkDevice device,
              VmaAllocator allocator,
              uint32_t width,
              uint32_t height,
              uint32_t mipLevels,
              VkFormat format,
              VkImageUsageFlags usage,
              VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
  void Cleanup(VkDevice device, VmaAllocator allocator);
};

// 2d texture sampled image
class Texture : public ImageBase {
public:
  Texture() = default;
  ~Texture() = default;
  Texture(VkDevice device,
          VmaAllocator allocator,
          uint32_t width,
          uint32_t height,
          uint32_t mipLevels,
          VkFormat format,
          VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
  void Create(VkDevice device,
              VmaAllocator allocator,
              uint32_t width,
              uint32_t height,
              uint32_t mipLevels,
              VkFormat format,
              VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
  void Cleanup(VkDevice device, VmaAllocator allocator);
};

}  // namespace hkr
