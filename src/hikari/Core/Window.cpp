#include "hikari/Core/Window.h"
#include "hikari/Util/Logger.h"

#include <GLFW/glfw3.h>

#include <string>

namespace hkr {

Window::Window(App* app, int width, int height, const std::string& name)
    : mApp(app), mSpec{.Width = width, .Height = height, .Name = name} {
  glfwSetErrorCallback([]([[maybe_unused]] int error, const char* description) {
    HKR_ERROR(description);
  });
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  mWindow = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(mWindow, app);
  SetCallback();
}

Window::~Window() {
  glfwDestroyWindow(mWindow);
  glfwTerminate();
}

void Window::SetCallback() {
  // glfwSetFramebufferSizeCallback(
  //     mWindow, [](GLFWwindow* window, int width, int height) {
  //       App& app = *static_cast<App*>(glfwGetWindowUserPointer(window));
  //     });
}

// ~Window();

// void Window::Init() {}

}  // namespace hkr
