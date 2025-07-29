#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_INTRINSICS
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>

namespace hkr {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using Mat2 = glm::mat2;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

// round up size to the multiple of alignment
inline uint32_t AlignedSize(uint32_t size, uint32_t alignment) {
  return (size + alignment - 1) / alignment * alignment;
}

// round up size to the multiple of alignment, alignment must be the power of
// two
inline uint32_t AlignedSizePowerOfTwo(uint32_t size, uint32_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

}  // namespace hkr
