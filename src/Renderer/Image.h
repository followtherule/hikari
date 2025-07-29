#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <string>

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
            VkSampleCountFlagBits numSamples,
            VkImageCreateFlags flags = 0,
            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);
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
              VkSampleCountFlagBits numSamples,
              VkImageCreateFlags flags = 0,
              VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);
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
  // create image and load data from file
  // TODO: may split out the staging part
  void Load(VkDevice device,
            VmaAllocator allocator,
            VkQueue queue,
            VkCommandPool commandPool,
            const std::string& fileName);
  void Cleanup(VkDevice device, VmaAllocator allocator);
};

// arrayLayers = 6
class Cubemap : public ImageBase {
public:
  Cubemap() = default;
  ~Cubemap() = default;
  Cubemap(VkDevice device,
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
  // create image and load data from file
  // TODO: may split out the staging part
  void Load(VkDevice device,
            VmaAllocator allocator,
            VkQueue queue,
            VkCommandPool commandPool,
            const std::string& fileName);
  void Cleanup(VkDevice device, VmaAllocator allocator);
};

// texture sampler
class SamplerBuilder {
public:
  SamplerBuilder();
  VkSampler Build(VkDevice device);
  SamplerBuilder& SetCreateFlags(VkSamplerCreateFlags flags);
  SamplerBuilder& SetMagFilter(VkFilter magFilter);
  SamplerBuilder& SetMinFilter(VkFilter minFilter);
  SamplerBuilder& SetMipmapMode(VkSamplerMipmapMode mipmapMode);
  SamplerBuilder& SetAddressModeU(VkSamplerAddressMode addressMode);
  SamplerBuilder& SetAddressModeV(VkSamplerAddressMode addressMode);
  SamplerBuilder& SetAddressModeW(VkSamplerAddressMode addressMode);
  SamplerBuilder& SetMipLodBias(float mipLodBias);
  SamplerBuilder& SetMaxAnisotropy(float maxAnisotropy);
  SamplerBuilder& SetCompareOp(VkCompareOp compareOp);
  SamplerBuilder& SetMinLod(float minLod);
  SamplerBuilder& SetMaxLod(float maxLod);
  SamplerBuilder& SetBorderColor(VkBorderColor borderColor);

private:
  VkSamplerCreateInfo mSamplerInfo{};
};

}  // namespace hkr
