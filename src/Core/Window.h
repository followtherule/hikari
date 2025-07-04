#pragma once

#include <string>

// forward declare
namespace hkr {
class App;
}
class GLFWwindow;

namespace hkr {

class Window {
public:
  ~Window();
  void Init(App* app, int width, int height, const char* appName);
  GLFWwindow* GetWindow() const { return mWindow; }

private:
  void CleanUp();
  void SetCallback();

  GLFWwindow* mWindow;
  App* mApp;

  struct {
    int Width;
    int Height;
  } mSpec;
  bool mResized = false;
};

}  // namespace hkr
