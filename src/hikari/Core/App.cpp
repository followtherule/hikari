#include "hikari/Core/App.h"
#include "hikari/Util/Logger.h"
#include "hikari/Renderer/Renderer.h"

#include <GLFW/glfw3.h>

namespace hkr {

App::App() {
  Logger::Init();
  mWindow = new Window(this, 1920, 1080, "hikari");
  mRenderer = new Renderer();
  mRenderer->Init(mWindow);
}

App::~App() {
  delete mRenderer;
  delete mWindow;
}

void App::Run() {
  while (!glfwWindowShouldClose(mWindow->GetWindow())) {
    glfwPollEvents();
    mRenderer->DrawFrame();
  }

  // vkDeviceWaitIdle(device);
}

}  // namespace hkr
