#pragma once

#include "hikari/hikari_export.hpp"

namespace hkr {

class Window;
class RenderEngine;

// default app settings
struct AppSettings {
  char* appName;
  char* assetPath;
  char* modelRelPath;
  char* cubemapRelPath;
  int width = 800;
  int height = 600;
  bool vsync = true;
};

class HKR_EXPORT App {
public:
  ~App();
  // Init the rendering engine
  void Init();
  // rendering loop
  void Run();

  AppSettings settings;

private:
  // clean up resources
  void HKR_NO_EXPORT Cleanup();
  void HKR_NO_EXPORT OnResize(int width, int height);
  void HKR_NO_EXPORT OnKeyEvent(int key, int action);
  void HKR_NO_EXPORT OnMouseEvent(int button, int action);
  void HKR_NO_EXPORT OnMouseMoveEvent(double x, double y);
  bool HKR_NO_EXPORT IsMinimized();

  Window* mWindow = nullptr;
  RenderEngine* mRenderEngine = nullptr;
  bool mIsRunning = false;
  bool mIsResizing = false;

  friend class Window;
  friend class RenderEngine;
};

}  // namespace hkr
