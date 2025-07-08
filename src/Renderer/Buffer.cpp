#include "Renderer/Buffer.h"
#include <vulkan/vulkan_core.h>
#include "Util/vk_util.h"

#include <cstring>

namespace hkr {

Buffer::Buffer(VmaAllocator allocator,
               VmaAllocationCreateFlags allocFlags,
               VkDeviceSize size,
               VkBufferUsageFlags usage) {
  Create(allocator, allocFlags, size, usage);
}

void Buffer::Create(VmaAllocator allocator,
                    VmaAllocationCreateFlags allocFlags,
                    VkDeviceSize size,
                    VkBufferUsageFlags usage) {
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

void Buffer::Cleanup(VmaAllocator allocator) {
  vmaDestroyBuffer(allocator, buffer, allocation);
}

MappableBuffer::MappableBuffer(VmaAllocator allocator,
                               VmaAllocationCreateFlags allocFlags,
                               VkDeviceSize size,
                               VkBufferUsageFlags usage) {
  Create(allocator, allocFlags, size, usage);
}

void MappableBuffer::Create(VmaAllocator allocator,
                            VmaAllocationCreateFlags allocFlags,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage) {
  Buffer::Create(allocator, allocFlags, size, usage);
}

void MappableBuffer::Cleanup(VmaAllocator allocator) {
  Buffer::Cleanup(allocator);
}

void MappableBuffer::Map(VmaAllocator allocator) {
  vmaMapMemory(allocator, allocation, &map);
}

void MappableBuffer::Unmap(VmaAllocator allocator) {
  vmaUnmapMemory(allocator, allocation);
}

void MappableBuffer::Write(void* data, size_t size) { memcpy(map, data, size); }

UniformBuffer::UniformBuffer(VmaAllocator allocator, VkDeviceSize size) {
  Create(allocator, size);
}

void UniformBuffer::Create(VmaAllocator allocator, VkDeviceSize size) {
  MappableBuffer::Create(
      allocator,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT,
      size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
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
      size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

void StagingBuffer::Cleanup(VmaAllocator allocator) {
  MappableBuffer::Cleanup(allocator);
}

VertexBuffer::VertexBuffer(VkDevice device,
                           VmaAllocator allocator,
                           VkQueue queue,
                           VkCommandPool commandPool,
                           void* data,
                           VkDeviceSize size) {
  Create(device, allocator, queue, commandPool, data, size);
}

void VertexBuffer::Create(VkDevice device,
                          VmaAllocator allocator,
                          VkQueue queue,
                          VkCommandPool commandPool,
                          void* data,
                          VkDeviceSize size) {
  StagingBuffer stagingBuffer;
  stagingBuffer.Create(allocator, size);
  stagingBuffer.Map(allocator);
  stagingBuffer.Write(data, size);
  stagingBuffer.Unmap(allocator);
  Buffer::Create(
      allocator, 0, size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  CopyBuffer(device, queue, commandPool, stagingBuffer.buffer, buffer, size);
  stagingBuffer.Cleanup(allocator);
}

void VertexBuffer::Cleanup(VmaAllocator allocator) {
  Buffer::Cleanup(allocator);
}

IndexBuffer::IndexBuffer(VkDevice device,
                         VmaAllocator allocator,
                         VkQueue queue,
                         VkCommandPool commandPool,
                         void* data,
                         VkDeviceSize size) {
  Create(device, allocator, queue, commandPool, data, size);
}

void IndexBuffer::Create(VkDevice device,
                         VmaAllocator allocator,
                         VkQueue queue,
                         VkCommandPool commandPool,
                         void* data,
                         VkDeviceSize size) {
  StagingBuffer stagingBuffer;
  stagingBuffer.Create(allocator, size);
  stagingBuffer.Map(allocator);
  stagingBuffer.Write(data, size);
  stagingBuffer.Unmap(allocator);
  Buffer::Create(
      allocator, 0, size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  CopyBuffer(device, queue, commandPool, stagingBuffer.buffer, buffer, size);
  stagingBuffer.Cleanup(allocator);
}

void IndexBuffer::Cleanup(VmaAllocator allocator) {
  Buffer::Cleanup(allocator);
}

}  // namespace hkr
