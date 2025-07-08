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

void TransitImageLayout(VkDevice device,
                        VkQueue queue,
                        VkCommandPool commandPool,
                        VkImage image,
                        VkImageLayout oldLayout,
                        VkImageLayout newLayout,
                        uint32_t mipLevels);

void GenerateMipmaps(VkDevice device,
                     VkQueue queue,
                     VkCommandPool commandPool,
                     VkImage image,
                     int32_t texWidth,
                     int32_t texHeight,
                     uint32_t mipLevels);

void CopyBufferToImage(VkDevice device,
                       VkQueue queue,
                       VkCommandPool commandPool,
                       VkBuffer buffer,
                       VkImage image,
                       uint32_t width,
                       uint32_t height);

void CopyBuffer(VkDevice device,
                VkQueue queue,
                VkCommandPool commandPool,
                VkBuffer srcBuffer,
                VkBuffer dstBuffer,
                VkDeviceSize size);

}  // namespace hkr
