#pragma once

#include "hikari/hikari_export.hpp"

namespace hkr {

class Window;
class Renderer;

class HKR_EXPORT App {
public:
  App();
  ~App();
  void Run();

private:
  Window* mWindow = nullptr;
  Renderer* mRenderer = nullptr;
  bool mIsRunning = false;
  bool mIsResizing = false;
};

}  // namespace hkr
