#pragma once

#include <volk.h>
#include <vector>
#include <string>
#include <span>

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
                        uint32_t mipLevels,
                        uint32_t arrayLayers = 1);

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

void CopyBufferToTexture(VkCommandBuffer commandBuffer,
                         VkBuffer buffer,
                         VkImage image,
                         std::span<VkBufferImageCopy2> copyRegions);

void CopyBufferToBuffer(VkCommandBuffer commandBuffer,
                        VkBuffer srcBuffer,
                        VkBuffer dstBuffer,
                        VkDeviceSize size,
                        VkDeviceSize srcOffset = 0,
                        VkDeviceSize dstOffset = 0);

void CopyImageToImage(VkCommandBuffer commandBuffer,
                      VkImage src,
                      VkImage dst,
                      VkExtent2D srcExtent,
                      VkExtent2D dstExtent);

VkShaderModule CreateShaderModule(VkDevice device,
                                  const std::vector<char>& code);

VkShaderModule LoadShaderModule(VkDevice device, const std::string& shaderFile);

VkDeviceAddress GetBufferDeviceAddress(VkDevice device, VkBuffer buffer);

uint32_t GetMipLevels(uint32_t width, uint32_t height);

}  // namespace hkr
