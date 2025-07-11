#pragma once

#include "Core/Math.h"

namespace hkr {

inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct UniformBufferObject {
  alignas(16) hkr::Mat4 model;
  alignas(16) hkr::Mat4 view;
  alignas(16) hkr::Mat4 proj;
};

}  // namespace hkr
