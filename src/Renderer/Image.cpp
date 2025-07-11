#include "Renderer/Image.h"
#include <vulkan/vulkan_core.h>
#include "Util/vk_debug.h"

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
                     VkSampleCountFlagBits numSamples) {
  Create(device, allocator, allocFlags, width, height, depth, mipLevels,
         arrayLayers, format, usage, numSamples);
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
                       VkSampleCountFlagBits numSamples) {
  // create image
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
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
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
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

void Texture::Cleanup(VkDevice device, VmaAllocator allocator) {
  ImageBase::Cleanup(device, allocator);
}

}  // namespace hkr
