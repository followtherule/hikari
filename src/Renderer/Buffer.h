#pragma once

#include <vk_mem_alloc.h>

namespace hkr {

// general buffer (may not be used directly)
class Buffer {
public:
  Buffer() = default;
  ~Buffer() = default;
  Buffer(VmaAllocator allocator,
         VmaAllocationCreateFlags allocFlags,
         VkDeviceSize size,
         VkBufferUsageFlags usage);
  void Create(VmaAllocator allocator,
              VmaAllocationCreateFlags allocFlags,
              VkDeviceSize size,
              VkBufferUsageFlags usage);

  void Cleanup(VmaAllocator allocator);

  VkBuffer buffer;
  VmaAllocation allocation;
};

// host visible buffer (may not be used directly)
class MappableBuffer : public Buffer {
public:
  MappableBuffer() = default;
  ~MappableBuffer() = default;
  MappableBuffer(VmaAllocator allocator,
                 VmaAllocationCreateFlags allocFlags,
                 VkDeviceSize size,
                 VkBufferUsageFlags usage);
  void Create(VmaAllocator allocator,
              VmaAllocationCreateFlags allocFlags,
              VkDeviceSize size,
              VkBufferUsageFlags usage);
  void Cleanup(VmaAllocator allocator);
  void Map(VmaAllocator allocator);
  void Unmap(VmaAllocator allocator);
  void Write(void* data, size_t size);

  void* map = nullptr;
};

// host visible, frequently update data
class UniformBuffer : public MappableBuffer {
public:
  UniformBuffer() = default;
  ~UniformBuffer() = default;
  UniformBuffer(VmaAllocator allocator, VkDeviceSize size);
  void Create(VmaAllocator allocator, VkDeviceSize size);
  void Cleanup(VmaAllocator allocator);
};

// for uploading data to gpu
class StagingBuffer : public MappableBuffer {
public:
  StagingBuffer() = default;
  ~StagingBuffer() = default;
  StagingBuffer(VmaAllocator allocator, VkDeviceSize size);
  void Create(VmaAllocator allocator, VkDeviceSize size);
  void Cleanup(VmaAllocator allocator);
};

class VertexBuffer : public Buffer {
public:
  VertexBuffer() = default;
  ~VertexBuffer() = default;
  VertexBuffer(VkDevice device,
               VmaAllocator allocator,
               VkQueue queue,
               VkCommandPool commandPool,
               void* data,
               VkDeviceSize size);
  void Create(VkDevice device,
              VmaAllocator allocator,
              VkQueue queue,
              VkCommandPool commandPool,
              void* data,
              VkDeviceSize size);
  void Cleanup(VmaAllocator allocator);
};

class IndexBuffer : public Buffer {
public:
  IndexBuffer() = default;
  ~IndexBuffer() = default;
  IndexBuffer(VkDevice device,
              VmaAllocator allocator,
              VkQueue queue,
              VkCommandPool commandPool,
              void* data,
              VkDeviceSize size);
  void Create(VkDevice device,
              VmaAllocator allocator,
              VkQueue queue,
              VkCommandPool commandPool,
              void* data,
              VkDeviceSize size);
  void Cleanup(VmaAllocator allocator);
};

// local buffer in gpu
// class StorageBuffer : public Buffer {
// public:
//   StorageBuffer() = default;
//   ~StorageBuffer() = default;
// };

}  // namespace hkr
