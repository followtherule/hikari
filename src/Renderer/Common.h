#pragma once

#include "Core/Math.h"

namespace hkr {

inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct UniformBufferObject {
  // view/proj for rasterizer, view inverse/proj inverse for raytracer
  alignas(16) Mat4 view;
  alignas(16) Mat4 proj;
  Vec4 viewPos;
  Vec3 lightPos;
  uint32_t frame = 0;  // for raytracing
};

}  // namespace hkr
