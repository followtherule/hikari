#include "Renderer/Image.h"
#include "Renderer/Buffer.h"
#include "Util/vk_debug.h"
#include "Util/vk_util.h"

#include <ktx.h>
#include <cstdint>

namespace {

VkImageAspectFlags GetAspectFlags(VkFormat format) {
  VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT;
  if (format >= VK_FORMAT_D16_UNORM) {
    flags = VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  if (format >= VK_FORMAT_D16_UNORM_S8_UINT) {
    flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  return flags;
}

}  // namespace

namespace hkr {

ImageBase::ImageBase(VkDevice device,
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
                     VkImageCreateFlags flags,
                     VkImageViewType viewType) {
  Create(device, allocator, allocFlags, width, height, depth, mipLevels,
         arrayLayers, format, usage, numSamples, flags, viewType);
}

void ImageBase::Create(VkDevice device,
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
                       VkImageCreateFlags flags,
                       VkImageViewType viewType) {
  // create image
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.flags = flags;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = depth;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = arrayLayers;
  imageInfo.format = format;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = numSamples;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  // allocate memory
  VmaAllocationCreateInfo allocInfo{};
  allocInfo.flags = allocFlags;
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.priority = 1.0f;

  vmaCreateImage(allocator, &imageInfo, &allocInfo, &image, &allocation,
                 nullptr);

  // create imageView
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = viewType;
  viewInfo.format = format;
  // viewInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
  //                        VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  viewInfo.subresourceRange.aspectMask = GetAspectFlags(format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = arrayLayers;

  VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &imageView));
}

void ImageBase::Cleanup(VkDevice device, VmaAllocator allocator) {
  vkDestroyImageView(device, imageView, nullptr);
  vmaDestroyImage(allocator, image, allocation);
}

Image::Image(VkDevice device,
             VmaAllocator allocator,
             uint32_t width,
             uint32_t height,
             uint32_t mipLevels,
             VkFormat format,
             VkImageUsageFlags usage,
             VkSampleCountFlagBits numSamples) {
  Create(device, allocator, width, height, mipLevels, format, usage,
         numSamples);
}

void Image::Create(VkDevice device,
                   VmaAllocator allocator,
                   uint32_t width,
                   uint32_t height,
                   uint32_t mipLevels,
                   VkFormat format,
                   VkImageUsageFlags usage,
                   VkSampleCountFlagBits numSamples) {
  ImageBase::Create(device, allocator,
                    VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, width, height,
                    1, mipLevels, 1, format, usage, numSamples);
}

void Image::Cleanup(VkDevice device, VmaAllocator allocator) {
  ImageBase::Cleanup(device, allocator);
}

Texture::Texture(VkDevice device,
                 VmaAllocator allocator,
                 uint32_t width,
                 uint32_t height,
                 uint32_t mipLevels,
                 VkFormat format,
                 VkSampleCountFlagBits numSamples) {
  Create(device, allocator, width, height, mipLevels, format, numSamples);
}

void Texture::Create(VkDevice device,
                     VmaAllocator allocator,
                     uint32_t width,
                     uint32_t height,
                     uint32_t mipLevels,
                     VkFormat format,
                     VkSampleCountFlagBits numSamples) {
  ImageBase::Create(
      device, allocator, 0, width, height, 1, mipLevels, 1, format,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_SAMPLED_BIT,
      numSamples);
}

void Texture::Load(VkDevice device,
                   VmaAllocator allocator,
                   VkQueue queue,
                   VkCommandPool commandPool,
                   const std::string& fileName) {
  size_t extensionPos = fileName.find_last_of(".");
  HKR_ASSERT(extensionPos != std::string::npos);
  std::string_view fileExtension = fileName.substr(extensionPos + 1);
  HKR_ASSERT(fileExtension == "ktx2");
  ktxTexture2* ktxTexture = nullptr;
  ktxResult result = ktxTexture2_CreateFromNamedFile(
      fileName.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
  HKR_ASSERT(result == KTX_SUCCESS);
  uint32_t width = ktxTexture->baseWidth;
  uint32_t height = ktxTexture->baseHeight;
  uint32_t mipLevels = ktxTexture->numLevels;
  auto format = static_cast<VkFormat>(ktxTexture->vkFormat);
  ktx_uint8_t* textureData = ktxTexture->pData;
  ktx_size_t textureSize = ktxTexture->dataSize;
  // create staging buffer and copy texture data
  StagingBuffer staging;
  staging.Create(allocator, textureSize);
  staging.Map(allocator);
  staging.Write(textureData, textureSize);
  staging.Unmap(allocator);
  // copyRegions for mipmaps
  std::vector<VkBufferImageCopy2> copyRegions(mipLevels);
  for (size_t i = 0; i < mipLevels; i++) {
    ktx_size_t offset;
    KTX_error_code result =
        ktxTexture2_GetImageOffset(ktxTexture, i, 0, 0, &offset);
    HKR_ASSERT(result == KTX_SUCCESS);
    VkBufferImageCopy2 copyRegion{};
    copyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
    copyRegion.bufferOffset = offset;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = i;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {std::max(width >> i, 1u),
                              std::max(height >> i, 1u), 1};
    copyRegions[i] = copyRegion;
  }
  // create image
  Create(device, allocator, width, height, mipLevels, format,
         VK_SAMPLE_COUNT_1_BIT);
  VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, commandPool);
  TransitImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
  CopyBufferToTexture(commandBuffer, staging.buffer, image, copyRegions);
  TransitImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);
  EndOneTimeCommands(device, queue, commandPool, commandBuffer);

  ktxTexture2_Destroy(ktxTexture);
  staging.Cleanup(allocator);
}

void Texture::Cleanup(VkDevice device, VmaAllocator allocator) {
  ImageBase::Cleanup(device, allocator);
}

Cubemap::Cubemap(VkDevice device,
                 VmaAllocator allocator,
                 uint32_t width,
                 uint32_t height,
                 uint32_t mipLevels,
                 VkFormat format,
                 VkSampleCountFlagBits numSamples) {
  Create(device, allocator, width, height, mipLevels, format, numSamples);
}

void Cubemap::Create(VkDevice device,
                     VmaAllocator allocator,
                     uint32_t width,
                     uint32_t height,
                     uint32_t mipLevels,
                     VkFormat format,
                     VkSampleCountFlagBits numSamples) {
  ImageBase::Create(
      device, allocator, 0, width, height, 1, mipLevels, 6, format,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_SAMPLED_BIT,
      numSamples, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_IMAGE_VIEW_TYPE_CUBE);
}

void Cubemap::Load(VkDevice device,
                   VmaAllocator allocator,
                   VkQueue queue,
                   VkCommandPool commandPool,
                   const std::string& fileName) {
  size_t extensionPos = fileName.find_last_of(".");
  HKR_ASSERT(extensionPos != std::string::npos);
  std::string_view fileExtension = fileName.substr(extensionPos + 1);
  HKR_ASSERT(fileExtension == "ktx2");
  ktxTexture2* ktxCubeMap = nullptr;
  ktxResult result = ktxTexture2_CreateFromNamedFile(
      fileName.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxCubeMap);
  HKR_ASSERT(result == KTX_SUCCESS);
  uint32_t width = ktxCubeMap->baseWidth;
  uint32_t height = ktxCubeMap->baseHeight;
  uint32_t mipLevels = ktxCubeMap->numLevels;
  auto format = static_cast<VkFormat>(ktxCubeMap->vkFormat);
  ktx_uint8_t* textureData = ktxCubeMap->pData;
  ktx_size_t textureSize = ktxCubeMap->dataSize;
  // create staging buffer and copy texture data
  StagingBuffer staging;
  staging.Create(allocator, textureSize);
  staging.Map(allocator);
  staging.Write(textureData, textureSize);
  staging.Unmap(allocator);
  // copyRegions for mipmaps
  std::vector<VkBufferImageCopy2> copyRegions(6 * mipLevels);
  for (size_t face = 0; face < 6; face++) {
    for (size_t level = 0; level < mipLevels; level++) {
      ktx_size_t offset;
      KTX_error_code result =
          ktxTexture2_GetImageOffset(ktxCubeMap, level, 0, face, &offset);
      HKR_ASSERT(result == KTX_SUCCESS);
      VkBufferImageCopy2 copyRegion{};
      copyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
      copyRegion.bufferOffset = offset;
      copyRegion.bufferRowLength = 0;
      copyRegion.bufferImageHeight = 0;
      copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.imageSubresource.mipLevel = level;
      copyRegion.imageSubresource.baseArrayLayer = face;
      copyRegion.imageSubresource.layerCount = 1;
      copyRegion.imageOffset = {0, 0, 0};
      copyRegion.imageExtent = {std::max(width >> level, 1u),
                                std::max(height >> level, 1u), 1};
      copyRegions[face * mipLevels + level] = copyRegion;
    }
  }
  // create image
  Create(device, allocator, width, height, mipLevels, format,
         VK_SAMPLE_COUNT_1_BIT);
  VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, commandPool);
  TransitImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, 6);
  CopyBufferToTexture(commandBuffer, staging.buffer, image, copyRegions);
  TransitImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, 6);
  EndOneTimeCommands(device, queue, commandPool, commandBuffer);

  ktxTexture2_Destroy(ktxCubeMap);
  staging.Cleanup(allocator);
}

void Cubemap::Cleanup(VkDevice device, VmaAllocator allocator) {
  ImageBase::Cleanup(device, allocator);
}

// default sampler
SamplerBuilder::SamplerBuilder() {
  mSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  mSamplerInfo.magFilter = VK_FILTER_LINEAR;
  // mSamplerInfo.magFilter = VK_FILTER_NEAREST;
  mSamplerInfo.minFilter = VK_FILTER_LINEAR;
  mSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  mSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  mSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  mSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  mSamplerInfo.mipLodBias = 0.0f;
  mSamplerInfo.anisotropyEnable = VK_FALSE;
  mSamplerInfo.maxAnisotropy = 1.0f;
  mSamplerInfo.compareEnable = VK_FALSE;
  mSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  mSamplerInfo.minLod = 0.0f;
  mSamplerInfo.maxLod = VK_LOD_CLAMP_NONE;
  mSamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  mSamplerInfo.unnormalizedCoordinates = VK_FALSE;
}

VkSampler SamplerBuilder::Build(VkDevice device) {
  VkSampler sampler;
  VK_CHECK(vkCreateSampler(device, &mSamplerInfo, nullptr, &sampler));
  return sampler;
}

SamplerBuilder& SamplerBuilder::SetCreateFlags(VkSamplerCreateFlags flags) {
  mSamplerInfo.flags = flags;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetMagFilter(VkFilter magFilter) {
  mSamplerInfo.magFilter = magFilter;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetMinFilter(VkFilter minFilter) {
  mSamplerInfo.minFilter = minFilter;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetMipmapMode(VkSamplerMipmapMode mipmapMode) {
  mSamplerInfo.mipmapMode = mipmapMode;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetAddressModeU(
    VkSamplerAddressMode addressMode) {
  mSamplerInfo.addressModeU = addressMode;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetAddressModeV(
    VkSamplerAddressMode addressMode) {
  mSamplerInfo.addressModeV = addressMode;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetAddressModeW(
    VkSamplerAddressMode addressMode) {
  mSamplerInfo.addressModeW = addressMode;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetMipLodBias(float mipLodBias) {
  mSamplerInfo.mipLodBias = mipLodBias;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetMaxAnisotropy(float maxAnisotropy) {
  mSamplerInfo.anisotropyEnable = VK_TRUE;
  mSamplerInfo.maxAnisotropy = maxAnisotropy;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetCompareOp(VkCompareOp compareOp) {
  mSamplerInfo.compareEnable = VK_TRUE;
  mSamplerInfo.compareOp = compareOp;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetMinLod(float minLod) {
  mSamplerInfo.minLod = minLod;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetMaxLod(float maxLod) {
  mSamplerInfo.maxLod = maxLod;
  return *this;
}

SamplerBuilder& SamplerBuilder::SetBorderColor(VkBorderColor borderColor) {
  mSamplerInfo.borderColor = borderColor;
  return *this;
}

}  // namespace hkr
