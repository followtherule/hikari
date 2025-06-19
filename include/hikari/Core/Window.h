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
  Window(App* app, int width, int height, const std::string& name);
  ~Window();
  GLFWwindow* GetWindow() const { return mWindow; }

private:
  void SetCallback();

  GLFWwindow* mWindow;
  App* mApp;

  struct Spec {
    int Width;
    int Height;
    std::string Name;
  } mSpec;
};

}  // namespace hkr
