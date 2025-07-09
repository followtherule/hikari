#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstdint>

namespace hkr {

// general image (may not be used directly)
class Image {
public:
  Image() = default;
  ~Image() = default;
  Image(VkDevice device,
        VmaAllocator allocator,
        VmaAllocatorCreateFlags allocFlags,
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        uint32_t mipLevels,
        uint32_t arrayLayers,
        VkFormat format,
        VkSampleCountFlagBits numSamples,
        VkImageUsageFlags usage);
  void Create(VkDevice device,
              VmaAllocator allocator,
              VmaAllocatorCreateFlags allocFlags,
              uint32_t width,
              uint32_t height,
              uint32_t depth,
              uint32_t mipLevels,
              uint32_t arrayLayers,
              VkFormat format,
              VkSampleCountFlagBits numSamples,
              VkImageUsageFlags usage);
  void Cleanup(VkDevice device, VmaAllocator allocator);

  VkImage image;
  VkImageView imageView;
  VmaAllocation allocation;
};

// 2d image (may not be used directly)
class Image2D : public Image {
public:
  Image2D() = default;
  ~Image2D() = default;
  Image2D(VkDevice device,
          VmaAllocator allocator,
          VmaAllocatorCreateFlags allocFlags,
          uint32_t width,
          uint32_t height,
          uint32_t mipLevels,
          VkFormat format,
          VkSampleCountFlagBits numSamples,
          VkImageUsageFlags usage);
  void Create(VkDevice device,
              VmaAllocator allocator,
              VmaAllocatorCreateFlags allocFlags,
              uint32_t width,
              uint32_t height,
              uint32_t mipLevels,
              VkFormat format,
              VkSampleCountFlagBits numSamples,
              VkImageUsageFlags usage);
  void Cleanup(VkDevice device, VmaAllocator allocator);
};

// storage color image/off-screen rendering target with dedicated memory
// allocation. See
// https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
class ColorImage2D : public Image2D {
public:
  ColorImage2D() = default;
  ~ColorImage2D() = default;
  ColorImage2D(VkDevice device,
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

// storage depth image/off-screen rendering target with dedicated allocation
class DepthImage2D : public Image2D {
public:
  DepthImage2D() = default;
  ~DepthImage2D() = default;
  DepthImage2D(VkDevice device,
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

// 2d texture, rendering target or sampled image
class Texture2D : public Image2D {
public:
  Texture2D() = default;
  ~Texture2D() = default;
  Texture2D(VkDevice device,
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
