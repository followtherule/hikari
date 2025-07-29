#include "Renderer/Cube.h"
#include "Util/vk_util.h"

namespace hkr {

void Cube::Create(VkDevice device,
                  VkQueue queue,
                  VkCommandPool commandPool,
                  VmaAllocator allocator,
                  VkBufferUsageFlags2 bufferUsageFlags) {
  std::array<CubeVertex, 8> vertexData;
  vertexData[0] = {{-1, 1, -1}};
  vertexData[1] = {{-1, -1, -1}};
  vertexData[2] = {{1, -1, -1}};
  vertexData[3] = {{1, 1, -1}};
  vertexData[4] = {{-1, 1, 1}};
  vertexData[5] = {{-1, -1, 1}};
  vertexData[6] = {{1, -1, 1}};
  vertexData[7] = {{1, 1, 1}};

  std::array<uint32_t, 6 * 6> indexData = {
      // clang-format off
    3, 2, 0, 0, 2, 1, // back
    4, 5, 7, 7, 5, 6, // front
    0, 1, 4, 4, 1, 5, // left
    7, 6, 3, 3, 6, 2, // right
    0, 4, 3, 3, 4, 7, // up
    1, 2, 5, 5, 2, 6, // down
      // clang-format on
  };
  size_t vertexDataSize = vertexData.size() * sizeof(CubeVertex);
  size_t indexDataSize = indexData.size() * sizeof(uint32_t);

  StagingBuffer vertexStaging;
  vertexStaging.Create(allocator, vertexDataSize);
  vertexStaging.Map(allocator);
  vertexStaging.Write(vertexData.data(), vertexDataSize);
  vertexStaging.Unmap(allocator);
  vertices.Create(allocator, vertexDataSize,
                  VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | bufferUsageFlags);
  StagingBuffer indexStaging;
  indexStaging.Create(allocator, indexDataSize);
  indexStaging.Map(allocator);
  indexStaging.Write(indexData.data(), indexDataSize);
  indexStaging.Unmap(allocator);
  indices.Create(allocator, indexDataSize,
                 VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | bufferUsageFlags);

  VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, commandPool);
  CopyBufferToBuffer(commandBuffer, vertexStaging.buffer, vertices.buffer,
                     vertexDataSize);
  CopyBufferToBuffer(commandBuffer, indexStaging.buffer, indices.buffer,
                     indexDataSize);
  EndOneTimeCommands(device, queue, commandPool, commandBuffer);
  vertexStaging.Cleanup(allocator);
  indexStaging.Cleanup(allocator);
}

void Cube::Draw() {}

void Cube::Cleanup(VmaAllocator allocator) {
  vertices.Cleanup(allocator);
  indices.Cleanup(allocator);
}

}  // namespace hkr
