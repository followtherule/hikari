#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

namespace hkr {

// general buffer (may not be used directly)
class BufferBase {
public:
  BufferBase() = default;
  ~BufferBase() = default;
  BufferBase(VmaAllocator allocator,
             VmaAllocationCreateFlags allocFlags,
             VkDeviceSize size,
             VkBufferUsageFlags2 usage);
  void Create(VmaAllocator allocator,
              VmaAllocationCreateFlags allocFlags,
              VkDeviceSize size,
              VkBufferUsageFlags2 usage);
  void Cleanup(VmaAllocator allocator);

  VkBuffer buffer;
  VmaAllocation allocation;
};

// host visible buffer (may not be used directly)
class MappableBuffer : public BufferBase {
public:
  MappableBuffer() = default;
  ~MappableBuffer() = default;
  MappableBuffer(VmaAllocator allocator,
                 VmaAllocationCreateFlags allocFlags,
                 VkDeviceSize size,
                 VkBufferUsageFlags2 usage);
  void Create(VmaAllocator allocator,
              VmaAllocationCreateFlags allocFlags,
              VkDeviceSize size,
              VkBufferUsageFlags2 usage);
  void Cleanup(VmaAllocator allocator);
  void* Map(VmaAllocator allocator);
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

// vertex/index/storage buffer
class Buffer : public BufferBase {
public:
  Buffer() = default;
  ~Buffer() = default;
  Buffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags2 usage);
  void Create(VmaAllocator allocator,
              VkDeviceSize size,
              VkBufferUsageFlags2 usage);
  void Create(VkDevice device,
              VmaAllocator allocator,
              VkQueue queue,
              VkCommandPool commandPool,
              void* data,
              VkDeviceSize size,
              VkBufferUsageFlags2 usage);
  void Cleanup(VmaAllocator allocator);
};

}  // namespace hkr
