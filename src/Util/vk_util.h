#pragma once

#include "Renderer/Image.h"

namespace hkr {

VkCommandBuffer BeginOneTimeCommands(VkDevice device,
                                     VkCommandPool commandPool);

void EndOneTimeCommands(VkDevice device,
                        VkQueue queue,
                        VkCommandPool commandPool,
                        VkCommandBuffer commandBuffer);

void InsertImageMemoryBarrier(VkCommandBuffer cmdbuffer,
                              VkImage image,
                              VkPipelineStageFlags srcStageMask,
                              VkPipelineStageFlags dstStageMask,
                              VkAccessFlags srcAccessMask,
                              VkAccessFlags dstAccessMask,
                              VkImageLayout oldImageLayout,
                              VkImageLayout newImageLayout,
                              VkImageSubresourceRange subresourceRange);

void TransitImageLayout(VkCommandBuffer commandBuffer,
                        VkImage image,
                        VkImageLayout oldLayout,
                        VkImageLayout newLayout,
                        uint32_t mipLevels);

void GenerateMipmaps(VkCommandBuffer commandBuffer,
                     VkImage image,
                     int32_t texWidth,
                     int32_t texHeight,
                     uint32_t mipLevels);

void CopyBufferToImage(VkCommandBuffer commandBuffer,
                       VkBuffer buffer,
                       VkImage image,
                       uint32_t width,
                       uint32_t height);

void CopyBufferToBuffer(VkCommandBuffer commandBuffer,
                        VkBuffer srcBuffer,
                        VkBuffer dstBuffer,
                        VkDeviceSize size);

void CopyImageToImage(VkCommandBuffer commandBuffer,
                      VkImage src,
                      VkImage dst,
                      VkExtent2D srcExtent,
                      VkExtent2D dstExtent);

}  // namespace hkr
