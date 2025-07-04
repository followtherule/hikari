#include "Core/Window.h"
#include "hikari/Util/Logger.h"
#include "hikari/Core/App.h"

#include <GLFW/glfw3.h>

namespace hkr {

void Window::Init(App* app, int width, int height, const char* appName) {
  mApp = app;
  mSpec.Width = width;
  mSpec.Height = height;
  glfwSetErrorCallback([]([[maybe_unused]] int error, const char* description) {
    HKR_ERROR(description);
  });
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  mWindow = glfwCreateWindow(width, height, appName, nullptr, nullptr);
  glfwSetWindowUserPointer(mWindow, app);
  SetCallback();
}

void Window::CleanUp() {
  glfwDestroyWindow(mWindow);
  glfwTerminate();
}

Window::~Window() { CleanUp(); }

void Window::SetCallback() {
  // resize
  glfwSetFramebufferSizeCallback(
      mWindow, [](GLFWwindow* window, int width, int height) {
        App& app = *static_cast<App*>(glfwGetWindowUserPointer(window));
        app.OnResize(width, height);
      });

  // mouse
  glfwSetMouseButtonCallback(
      mWindow, [](GLFWwindow* window, int button, int action, int mods) {
        App& app = *static_cast<App*>(glfwGetWindowUserPointer(window));
        app.OnMouseEvent(button, action);
      });

  // mouse pos
  glfwSetCursorPosCallback(
      mWindow, [](GLFWwindow* window, double xpos, double ypos) {
        App& app = *static_cast<App*>(glfwGetWindowUserPointer(window));
        app.OnMouseMoveEvent(xpos, ypos);
      });

  // key
  glfwSetKeyCallback(mWindow, [](GLFWwindow* window, int key, int scancode,
                                 int action, int mods) {
    App& app = *static_cast<App*>(glfwGetWindowUserPointer(window));
    app.OnKeyEvent(key, action);
  });
}

}  // namespace hkr
