#include "hikari/Core/App.h"
#include "Core/Window.h"

#include "hikari/Util/Logger.h"
#include "Renderer/RenderEngine.h"

#include <GLFW/glfw3.h>

namespace hkr {

void App::Init() {
  Logger::Init();
  mWindow = new Window;
  mWindow->Init(this, 1920, 1080, settings.appName);
  mRenderEngine = new RenderEngine;
  mRenderEngine->Init(settings, mWindow->GetWindow());
}

void App::Cleanup() {
  mRenderEngine->Cleanup();
  delete mRenderEngine;
  mWindow->Cleanup();
  delete mWindow;
}

App::~App() { Cleanup(); }

void App::Run() {
  while (!glfwWindowShouldClose(mWindow->GetWindow())) {
    glfwPollEvents();

    mRenderEngine->Render();
  }
}

bool App::IsMinimized() { return settings.width == 0 || settings.height == 0; }

void App::OnResize(int width, int height) {
  settings.width = width;
  settings.height = height;
  mRenderEngine->OnResize(width, height);
}

void App::OnKeyEvent(int key, int action) {
  mRenderEngine->OnKeyEvent(key, action);
}

void App::OnMouseEvent(int button, int action) {
  mRenderEngine->OnMouseEvent(button, action);
}

void App::OnMouseMoveEvent(double x, double y) {
  mRenderEngine->OnMouseMoveEvent(x, y);
}

}  // namespace hkr
