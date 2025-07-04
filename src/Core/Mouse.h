#pragma once

#include "Core/Math.h"

namespace hkr {

class Mouse {
public:
  struct {
    bool Left = false;
    bool Middle = false;
    bool Right = false;
  } State;
  Vec2 Position = Vec2(1.0f);
};

}  // namespace hkr
