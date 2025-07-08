#pragma once

#include "Core/Math.h"
#include "Core/Mouse.h"
#include "Renderer/Camera.h"
#include "Renderer/Image.h"
#include "Renderer/Buffer.h"
#include "hikari/Core/App.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <optional>
#include <vector>
#include <string>

class GLFWwindow;

namespace hkr {

struct Vertex {
  Vec3 pos;
  Vec3 color;
  Vec2 texCoord;

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3>
  getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    return attributeDescriptions;
  }

  bool operator==(const Vertex& other) const {
    return pos == other.pos && color == other.color &&
           texCoord == other.texCoord;
  }
};

class Renderer {
public:
  ~Renderer();
  // init vulkan rendering engine
  void Init(const AppSettings& settings, GLFWwindow* window);

  void DrawFrame();
  void Resize(int width, int height);

  void OnKeyEvent(int key, int action);
  void OnMouseEvent(int button, int action);
  void OnMouseMoveEvent(double x, double y);

  void Render();

private:
  // clean up resources
  void Cleanup();

  // create instance, choose physical device, build logical device, get graphics
  // queue, setup vma allocator
  void InitVulkan();

  void CreateSwapchain();
  void RecreateSwapchain();

  // off-screen images for the first renderpass
  // void CreateColorImage();
  // void CreateDepthImage();

  void CreateCommandPool();
  void CreateCommandBuffers();

  void CreatePipelineCache();
  void CreateDescriptorSetLayout();
  void CreateGraphicsPipeline();

  void CreateTextureImage();
  void CreateTextureSampler();
  void LoadModel();
  void CreateVertexBuffer();
  void CreateIndexBuffer();
  void CreateUniformBuffers();
  void CreateDescriptorPool();
  void CreateDescriptorSets();
  void CreateSyncObjects();

  // void CreateImage(VkImage& image,
  //                  VmaAllocation& imageAlloc,
  //                  uint32_t width,
  //                  uint32_t height,
  //                  uint32_t mipLevels,
  //                  VkSampleCountFlagBits numSamples,
  //                  VkFormat format,
  //                  VkImageTiling tiling,
  //                  VkImageUsageFlags usage,
  //                  VmaAllocatorCreateFlags allocFlags = 0);
  // VkImageView CreateImageView(VkImage image,
  //                             VkFormat format,
  //                             VkImageAspectFlags aspectFlags,
  //                             uint32_t mipLevels);
  void CreateBuffer(VkBuffer& buffer,
                    VmaAllocation& alloc,
                    VkDeviceSize size,
                    VkBufferUsageFlags usage,
                    VmaAllocationCreateFlags allocFlags = 0);

  // VkSampleCountFlagBits GetMaxUsableSampleCount();
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

  VkFormat FindDepthFormat();
  VkShaderModule CreateShaderModule(const std::vector<char>& code);
  // void TransitionImageLayout(VkImage image,
  //                            VkImageLayout oldLayout,
  //                            VkImageLayout newLayout,
  //                            uint32_t mipLevels);
  // void CopyBufferToImage(VkBuffer buffer,
  //                        VkImage image,
  //                        uint32_t width,
  //                        uint32_t height);
  // VkCommandBuffer BeginSingleTimeCommands();
  // void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
  // void GenerateMipmaps(VkImage image,
  //                      VkFormat imageFormat,
  //                      int32_t texWidth,
  //                      int32_t texHeight,
  //                      uint32_t mipLevels);
  // void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void CleanupSwapChain();
  void UpdateUniformBuffer(uint32_t currentImage);
  void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

  void InitCamera();

private:
  constexpr static int MAX_FRAMES_IN_FLIGHT = 2;
  std::string mModelPath;
  std::string mTexturePath;
  std::string mShaderPath;
  char* mAppName;
  GLFWwindow* mWindow;
  int mWidth = 0;
  int mHeight = 0;
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
  VkFormat mDepthFormat;
  std::vector<VkImage> mSwapchainImages;
  std::vector<VkImageView> mSwapchainImageViews;
  // VkExtent2D mSwapChainExtent;

  // off-screen images
  ColorImage2D mColorImage;
  DepthImage2D mDepthImage;

  // VkImage mColorImage;
  // VkImageView mColorImageView;
  // VmaAllocation mColorImageAlloc;
  // VkImage mDepthImage;
  // VkImageView mDepthImageView;
  // VmaAllocation mDepthImageAlloc;
  bool mRequireStencil = false;

  VkCommandPool mCommandPool;
  std::vector<VkCommandBuffer> mCommandBuffers;

  // descriptor resources
  // std::vector<VkBuffer> mUniformBuffers;
  // std::vector<VmaAllocation> mUniformBuffersAlloc;
  // std::vector<void*> mUniformBuffersMapped;
  std::array<UniformBuffer, MAX_FRAMES_IN_FLIGHT> mUniformBuffers;

  Texture2D mTextureImage;
  // VkImage mTextureImage;
  // VmaAllocation mTextureImageAlloc;
  // VkImageView mTextureImageView;

  VkSampler mTextureSampler;
  std::vector<Vertex> mVertices;
  std::vector<uint32_t> mIndices;
  VertexBuffer mVertexBuffer;
  IndexBuffer mIndexBuffer;

  // VkBuffer mVertexBuffer;
  // VmaAllocation mVertexBufferAlloc;
  // VkBuffer mIndexBuffer;
  // VmaAllocation mIndexBufferAlloc;

  VkDescriptorSetLayout mDescriptorSetLayout;
  VkDescriptorPool mDescriptorPool;
  std::vector<VkDescriptorSet> mDescriptorSets;

  VkPipelineCache mPipelineCache{VK_NULL_HANDLE};
  VkPipelineLayout mPipelineLayout;
  VkPipeline mGraphicsPipeline;

  // sync primitives
  std::vector<VkSemaphore> mImageAvailableSemaphores;
  std::vector<VkSemaphore> mRenderFinishedSemaphores;
  std::vector<VkFence> mInFlightFences;

  uint32_t mCurrentFrame = 0;
  bool mFramebufferResized = false;

  Camera mCamera;
  Mouse mMouse;

  // VkSampleCountFlagBits mMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
};

}  // namespace hkr
