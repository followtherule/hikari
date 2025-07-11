#pragma once

#include "Core/Math.h"
#include "Core/Mouse.h"
#include "Renderer/Camera.h"
#include "Renderer/Image.h"
#include "Renderer/Buffer.h"
#include "hikari/Core/App.h"

#include "Renderer/Rasterizer.h"

#include <vector>
#include <string>

class GLFWwindow;

namespace hkr {

class RenderEngine {
public:
  // init vulkan rendering engine
  void Init(const AppSettings& settings, GLFWwindow* window);
  // clean up resources
  void Cleanup();

  // event
  void OnResize(int width, int height);
  void OnKeyEvent(int key, int action);
  void OnMouseEvent(int button, int action);
  void OnMouseMoveEvent(double x, double y);

  // the render loop
  void Render();

private:
  // create instance, choose physical device, build logical device, get graphics
  // queue, setup vma allocator
  void InitVulkan();

  void CreateSwapchain();
  void RecreateSwapchain();

  void CreateCommandPool();
  void CreateCommandBuffers();

  void CreateUniformBuffers();
  void CreateSyncObjects();

  void InitImGui();
  void CleanupImGui();
  void DrawFrame();
  void DrawImGui(VkCommandBuffer commandBuffer, uint32_t imageIndex);
  void UpdateUniformBuffer(uint32_t currentImage);
  void RecordCommandBuffer(VkCommandBuffer commandBuffer,
                           uint32_t currentFrame,
                           uint32_t imageIndex);

  void InitCamera();

private:
  std::string mModelPath;
  std::string mTexturePath;
  std::string mShaderPath;
  char* mAppName;
  GLFWwindow* mWindow;
  int mWidth = 0;
  int mHeight = 0;
  // Vec4 mClearColor{0.2f, 0.3f, 0.3f, 0.0f};
  bool mVsync = false;

  VkInstance mInstance;
  VkDebugUtilsMessengerEXT mDebugMessenger;
  VkSurfaceKHR mSurface;
  VkPhysicalDevice mPhysDevice;
  VkDevice mDevice;
  VmaAllocator mAllocator;

  VkQueue mGraphicsQueue;
  uint32_t mGraphicsFamilyIndex;

  VkSwapchainKHR mSwapchain;
  VkFormat mSwapchainImageFormat;
  std::vector<VkImage> mSwapchainImages;
  std::vector<VkImageView> mSwapchainImageViews;
  // VkExtent2D mSwapChainExtent;

  VkCommandPool mCommandPool;
  std::vector<VkCommandBuffer> mCommandBuffers;

  // descriptor resources
  std::array<UniformBuffer, MAX_FRAMES_IN_FLIGHT> mUniformBuffers;
  VkDescriptorPool mImGuiDescriptorPool;

  // sync primitives
  std::vector<VkSemaphore> mImageAvailableSemaphores;
  std::vector<VkSemaphore> mRenderFinishedSemaphores;
  std::vector<VkFence> mInFlightFences;

  uint32_t mCurrentFrame = 0;
  bool mFramebufferResized = false;

  Camera mCamera;
  Mouse mMouse;

  // VkSampleCountFlagBits mMsaaSamples = VK_SAMPLE_COUNT_1_BIT;

  // rasterizer
  Rasterizer* mRasterizer = nullptr;
};

}  // namespace hkr
