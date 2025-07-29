#include "Util/vk_util.h"
#include "Util/vk_debug.h"
#include "Util/Filesystem.h"

#include <ktx.h>

namespace hkr {

VkCommandBuffer BeginOneTimeCommands(VkDevice device,
                                     VkCommandPool commandPool) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void EndOneTimeCommands(VkDevice device,
                        VkQueue queue,
                        VkCommandPool commandPool,
                        VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkCommandBufferSubmitInfo commandInfo{};
  commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  commandInfo.pNext = nullptr;
  commandInfo.commandBuffer = commandBuffer;
  commandInfo.deviceMask = 0;

  VkSubmitInfo2 submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submitInfo.commandBufferInfoCount = 1;
  submitInfo.pCommandBufferInfos = &commandInfo;

  VK_CHECK(vkQueueSubmit2(queue, 1, &submitInfo, VK_NULL_HANDLE));
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void InsertImageMemoryBarrier(VkCommandBuffer commandbuffer,
                              VkImage image,
                              VkPipelineStageFlags srcStageMask,
                              VkPipelineStageFlags dstStageMask,
                              VkAccessFlags srcAccessMask,
                              VkAccessFlags dstAccessMask,
                              VkImageLayout oldImageLayout,
                              VkImageLayout newImageLayout,
                              VkImageSubresourceRange subresourceRange) {
  VkImageMemoryBarrier2 imageMemoryBarrier{};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.srcStageMask = srcStageMask;
  imageMemoryBarrier.srcAccessMask = srcAccessMask;
  imageMemoryBarrier.dstStageMask = dstStageMask;
  imageMemoryBarrier.dstAccessMask = dstAccessMask;
  imageMemoryBarrier.oldLayout = oldImageLayout;
  imageMemoryBarrier.newLayout = newImageLayout;
  imageMemoryBarrier.image = image;
  imageMemoryBarrier.subresourceRange = subresourceRange;

  VkDependencyInfo dependInfo{};
  dependInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependInfo.pNext = nullptr;
  dependInfo.imageMemoryBarrierCount = 1;
  dependInfo.pImageMemoryBarriers = &imageMemoryBarrier;

  vkCmdPipelineBarrier2(commandbuffer, &dependInfo);
}

void TransitImageLayout(VkCommandBuffer commandBuffer,
                        VkImage image,
                        VkImageLayout oldLayout,
                        VkImageLayout newLayout,
                        uint32_t mipLevels,
                        uint32_t arrayLayers) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  VkImageSubresourceRange subresourceRange{};
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = mipLevels;
  subresourceRange.baseArrayLayer = 0;
  subresourceRange.layerCount = arrayLayers;

  VkPipelineStageFlags2 srcStage;
  VkPipelineStageFlags2 dstStage;
  VkAccessFlags2 srcAccess;
  VkAccessFlags2 dstAccess;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    srcAccess = 0;
    dstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
  }
  InsertImageMemoryBarrier(commandBuffer, image, srcStage, dstStage, srcAccess,
                           dstAccess, oldLayout, newLayout, subresourceRange);
}

void GenerateMipmaps(VkCommandBuffer commandBuffer,
                     VkImage image,
                     int32_t texWidth,
                     int32_t texHeight,
                     uint32_t mipLevels) {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkDependencyInfo dependInfo{};
  dependInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependInfo.pNext = nullptr;
  dependInfo.imageMemoryBarrierCount = 1;
  dependInfo.pImageMemoryBarriers = &barrier;

  int32_t mipWidth = texWidth;
  int32_t mipHeight = texHeight;

  for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier2(commandBuffer, &dependInfo);

    VkImageBlit2 blit{};
    blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {std::max(mipWidth >> (i - 1), 1),
                          std::max(mipHeight >> (i - 1), 1), 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {std::max(mipWidth >> i, 1),
                          std::max(mipHeight >> i, 1), 1};

    VkBlitImageInfo2 blitInfo{};
    blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blitInfo.srcImage = image;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.dstImage = image;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.filter = VK_FILTER_LINEAR;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blit;

    vkCmdBlitImage2(commandBuffer, &blitInfo);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    vkCmdPipelineBarrier2(commandBuffer, &dependInfo);
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

  vkCmdPipelineBarrier2(commandBuffer, &dependInfo);
}

void CopyBufferToImage(VkCommandBuffer commandBuffer,
                       VkBuffer buffer,
                       VkImage image,
                       uint32_t width,
                       uint32_t height) {
  VkBufferImageCopy2 copyRegion{};
  copyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
  copyRegion.bufferOffset = 0;
  copyRegion.bufferRowLength = 0;
  copyRegion.bufferImageHeight = 0;
  copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.imageSubresource.mipLevel = 0;
  copyRegion.imageSubresource.baseArrayLayer = 0;
  copyRegion.imageSubresource.layerCount = 1;
  copyRegion.imageOffset = {0, 0, 0};
  copyRegion.imageExtent = {width, height, 1};
  VkCopyBufferToImageInfo2 copyInfo{};
  copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
  copyInfo.srcBuffer = buffer;
  copyInfo.dstImage = image;
  copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  copyInfo.regionCount = 1;
  copyInfo.pRegions = &copyRegion;
  vkCmdCopyBufferToImage2(commandBuffer, &copyInfo);
}

void CopyBufferToTexture(VkCommandBuffer commandBuffer,
                         VkBuffer buffer,
                         VkImage image,
                         std::span<VkBufferImageCopy2> copyRegions) {
  VkCopyBufferToImageInfo2 copyInfo{};
  copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
  copyInfo.srcBuffer = buffer;
  copyInfo.dstImage = image;
  copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  copyInfo.regionCount = static_cast<uint32_t>(copyRegions.size());
  copyInfo.pRegions = copyRegions.data();
  vkCmdCopyBufferToImage2(commandBuffer, &copyInfo);
}

void CopyBufferToBuffer(VkCommandBuffer commandBuffer,
                        VkBuffer srcBuffer,
                        VkBuffer dstBuffer,
                        VkDeviceSize size,
                        VkDeviceSize srcOffset,
                        VkDeviceSize dstOffset) {
  VkBufferCopy2 copyRegion{};
  copyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
  copyRegion.size = size;
  copyRegion.srcOffset = srcOffset;
  copyRegion.dstOffset = dstOffset;
  VkCopyBufferInfo2 copyInfo{};
  copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
  copyInfo.srcBuffer = srcBuffer;
  copyInfo.dstBuffer = dstBuffer;
  copyInfo.regionCount = 1;
  copyInfo.pRegions = &copyRegion;
  vkCmdCopyBuffer2(commandBuffer, &copyInfo);
}

void CopyImageToImage(VkCommandBuffer commandBuffer,
                      VkImage src,
                      VkImage dst,
                      VkExtent2D srcExtent,
                      VkExtent2D dstExtent) {
  VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                          .pNext = nullptr};

  blitRegion.srcOffsets[0] = {0, 0, 0};
  blitRegion.srcOffsets[1].x = srcExtent.width;
  blitRegion.srcOffsets[1].y = srcExtent.height;
  blitRegion.srcOffsets[1].z = 1;

  blitRegion.srcOffsets[0] = {0, 0, 0};
  blitRegion.dstOffsets[1].x = dstExtent.width;
  blitRegion.dstOffsets[1].y = dstExtent.height;
  blitRegion.dstOffsets[1].z = 1;

  blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blitRegion.srcSubresource.baseArrayLayer = 0;
  blitRegion.srcSubresource.layerCount = 1;
  blitRegion.srcSubresource.mipLevel = 0;

  blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blitRegion.dstSubresource.baseArrayLayer = 0;
  blitRegion.dstSubresource.layerCount = 1;
  blitRegion.dstSubresource.mipLevel = 0;

  VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                            .pNext = nullptr};
  blitInfo.dstImage = dst;
  blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blitInfo.srcImage = src;
  blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blitInfo.filter = VK_FILTER_LINEAR;
  blitInfo.regionCount = 1;
  blitInfo.pRegions = &blitRegion;

  vkCmdBlitImage2(commandBuffer, &blitInfo);
}

VkShaderModule CreateShaderModule(VkDevice device,
                                  const std::vector<char>& code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shaderModule;
  VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

  return shaderModule;
}

VkShaderModule LoadShaderModule(VkDevice device,
                                const std::string& shaderFile) {
  auto code = ReadFile(shaderFile);
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shaderModule;
  VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

  return shaderModule;
}

VkDeviceAddress GetBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
  VkBufferDeviceAddressInfoKHR addressInfo{};
  addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  addressInfo.buffer = buffer;
  return vkGetBufferDeviceAddressKHR(device, &addressInfo);
}

uint32_t GetMipLevels(uint32_t width, uint32_t height) {
  return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) +
         1;
}

}  // namespace hkr
