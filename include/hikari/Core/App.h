#pragma once

#include "hikari/hikari_export.hpp"

namespace hkr {

class Window;
class Renderer;

// default app settings
struct AppSettings {
  char* AppName;
  char* AssetPath;
  char* ModelRelPath;
  char* TextureRelPath;
  int Width = 800;
  int Height = 600;
  bool Vsync = true;
};

class HKR_EXPORT App {
public:
  ~App();
  // init the rendering engine
  void Init();
  // rendering loop
  void Run();

  AppSettings Settings;

private:
  // clean up resources
  void HKR_NO_EXPORT Cleanup();
  void HKR_NO_EXPORT OnResize(int width, int height);
  void HKR_NO_EXPORT OnKeyEvent(int key, int action);
  void HKR_NO_EXPORT OnMouseEvent(int button, int action);
  void HKR_NO_EXPORT OnMouseMoveEvent(double x, double y);
  bool HKR_NO_EXPORT IsMinimized();

  Window* mWindow = nullptr;
  Renderer* mRenderer = nullptr;
  bool mIsRunning = false;
  bool mIsResizing = false;

  friend class Window;
  friend class Renderer;
};

}  // namespace hkr
