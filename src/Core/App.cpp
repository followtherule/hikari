#include "hikari/Core/App.h"
#include "Core/Window.h"

#include "hikari/Util/Logger.h"
#include "Renderer/Renderer.h"

#include <GLFW/glfw3.h>

namespace hkr {

void App::Init() {
  Logger::Init();
  mWindow = new Window;
  mWindow->Init(this, 1920, 1080, settings.appName);
  mRenderer = new Renderer;
  mRenderer->Init(settings, mWindow->GetWindow());
}

void App::Cleanup() {
  delete mRenderer;
  delete mWindow;
}

App::~App() { Cleanup(); }

void App::Run() {
  while (!glfwWindowShouldClose(mWindow->GetWindow())) {
    glfwPollEvents();

    // if (IsMinimized()) {
    //   continue;
    // }

    mRenderer->Render();
  }

  // vkDeviceWaitIdle(device);
}

bool App::IsMinimized() { return settings.width == 0 || settings.height == 0; }

void App::OnResize(int width, int height) {
  settings.width = width;
  settings.height = height;
  mRenderer->Resize(width, height);
}

void App::OnKeyEvent(int key, int action) {
  mRenderer->OnKeyEvent(key, action);
}

void App::OnMouseEvent(int button, int action) {
  mRenderer->OnMouseEvent(button, action);
}

void App::OnMouseMoveEvent(double x, double y) {
  mRenderer->OnMouseMoveEvent(x, y);
}

}  // namespace hkr
