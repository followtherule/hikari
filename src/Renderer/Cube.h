#pragma once

#include "Renderer/Buffer.h"
#include "Core/Math.h"

namespace hkr {

struct CubeVertex {
  Vec3 pos;
};

// 3d cube mesh (vertex/index buffer)
class Cube {
public:
  void Create(VkDevice device,
              VkQueue queue,
              VkCommandPool commandPool,
              VmaAllocator allocator,
              VkBufferUsageFlags2 bufferUsageFlags = 0);
  void Draw();
  void Cleanup(VmaAllocator allocator);

  Buffer vertices;
  Buffer indices;
};

}  // namespace hkr
