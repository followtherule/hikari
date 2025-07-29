#include "Renderer/Buffer.h"
#include "Util/vk_util.h"

#include <cstring>

namespace hkr {

BufferBase::BufferBase(VmaAllocator allocator,
                       VmaAllocationCreateFlags allocFlags,
                       VkDeviceSize size,
                       VkBufferUsageFlags2 usage) {
  Create(allocator, allocFlags, size, usage);
}

void BufferBase::Create(VmaAllocator allocator,
                        VmaAllocationCreateFlags allocFlags,
                        VkDeviceSize size,
                        VkBufferUsageFlags2 usage) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.flags = allocFlags;
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

  VmaAllocationInfo allocInfo;
  vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &buffer,
                  &allocation, &allocInfo);
}

void BufferBase::Cleanup(VmaAllocator allocator) {
  vmaDestroyBuffer(allocator, buffer, allocation);
}

MappableBuffer::MappableBuffer(VmaAllocator allocator,
                               VmaAllocationCreateFlags allocFlags,
                               VkDeviceSize size,
                               VkBufferUsageFlags2 usage) {
  Create(allocator, allocFlags, size, usage);
}

void MappableBuffer::Create(VmaAllocator allocator,
                            VmaAllocationCreateFlags allocFlags,
                            VkDeviceSize size,
                            VkBufferUsageFlags2 usage) {
  BufferBase::Create(allocator, allocFlags, size, usage);
}

void MappableBuffer::Cleanup(VmaAllocator allocator) {
  BufferBase::Cleanup(allocator);
}

void* MappableBuffer::Map(VmaAllocator allocator) {
  vmaMapMemory(allocator, allocation, &map);
  return map;
}

void MappableBuffer::Unmap(VmaAllocator allocator) {
  vmaUnmapMemory(allocator, allocation);
}

void MappableBuffer::Write(void* data, size_t size, size_t offset) {
  memcpy((char*)map + offset, data, size);
}

UniformBuffer::UniformBuffer(VmaAllocator allocator, VkDeviceSize size) {
  Create(allocator, size);
}

void UniformBuffer::Create(VmaAllocator allocator, VkDeviceSize size) {
  MappableBuffer::Create(
      allocator,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT,
      size, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT);
}

void UniformBuffer::Cleanup(VmaAllocator allocator) {
  MappableBuffer::Cleanup(allocator);
}

StagingBuffer::StagingBuffer(VmaAllocator allocator, VkDeviceSize size) {
  Create(allocator, size);
}

void StagingBuffer::Create(VmaAllocator allocator, VkDeviceSize size) {
  MappableBuffer::Create(
      allocator,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT,
      size, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT);
}

void StagingBuffer::Cleanup(VmaAllocator allocator) {
  MappableBuffer::Cleanup(allocator);
}

Buffer::Buffer(VmaAllocator allocator,
               VkDeviceSize size,
               VkBufferUsageFlags2 usage) {
  Create(allocator, size, usage);
}

void Buffer::Create(VmaAllocator allocator,
                    VkDeviceSize size,
                    VkBufferUsageFlags2 usage) {
  BufferBase::Create(allocator, 0, size,
                     VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | usage);
}

void Buffer::Create(VmaAllocator allocator,
                    VkCommandBuffer commandBuffer,
                    StagingBuffer& stagingBuffer,
                    void* data,
                    VkDeviceSize size,
                    VkBufferUsageFlags2 usage) {
  stagingBuffer.Create(allocator, size);
  stagingBuffer.Map(allocator);
  stagingBuffer.Write(data, size);
  stagingBuffer.Unmap(allocator);
  BufferBase::Create(allocator, 0, size,
                     VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | usage);

  CopyBufferToBuffer(commandBuffer, stagingBuffer.buffer, buffer, size);
}

void Buffer::Create(VkDevice device,
                    VmaAllocator allocator,
                    VkQueue queue,
                    VkCommandPool commandPool,
                    void* data,
                    VkDeviceSize size,
                    VkBufferUsageFlags2 usage) {
  StagingBuffer stagingBuffer;
  stagingBuffer.Create(allocator, size);
  stagingBuffer.Map(allocator);
  stagingBuffer.Write(data, size);
  stagingBuffer.Unmap(allocator);
  BufferBase::Create(allocator, 0, size,
                     VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | usage);

  VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, commandPool);
  CopyBufferToBuffer(commandBuffer, stagingBuffer.buffer, buffer, size);
  EndOneTimeCommands(device, queue, commandPool, commandBuffer);
  stagingBuffer.Cleanup(allocator);
}

void Buffer::Cleanup(VmaAllocator allocator) { BufferBase::Cleanup(allocator); }

}  // namespace hkr
