#pragma once

#include "hikari/Core/Window.h"

#include <vulkan/vulkan_core.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <optional>
#include <vector>

namespace hkr {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

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
  void Init(Window* window);
  void DrawFrame();

private:
  void Cleanup();
  // setup vulkan rendering engine
  void CreateInstance();
  void CreateDebugMessenger();
  void CreateSurface();
  void ChoosePhysicalDevice();
  void CreateLogicalDevice();
  void CreateSwapChain();
  void CreateImageViews();
  void CreateRenderPass();
  void CreateDescriptorSetLayout();
  void CreateGraphicsPipeline();
  void CreateCommandPool();
  void CreateColorResources();
  void CreateDepthResources();
  void CreateFramebuffers();
  void CreateTextureImage();
  void CreateTextureImageView();
  void CreateTextureSampler();
  void LoadModel();
  void CreateVertexBuffer();
  void CreateIndexBuffer();
  void CreateUniformBuffers();
  void CreateDescriptorPool();
  void CreateDescriptorSets();
  void CreateCommandBuffers();
  void CreateSyncObjects();

  bool CheckValidationLayerSupport();
  std::vector<const char*> GetRequiredExtensions();
  void PopulateDebugMessengerCreateInfo(
      VkDebugUtilsMessengerCreateInfoEXT& createInfo);
  bool IsDeviceSuitable(VkPhysicalDevice device);
  VkSampleCountFlagBits GetMaxUsableSampleCount();
  struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
      return graphicsFamily.has_value() && presentFamily.has_value();
    }
  };
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
  struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
  };
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR>& availableFormats);
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR>& availablePresentModes);
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
  VkImageView CreateImageView(VkImage image,
                              VkFormat format,
                              VkImageAspectFlags aspectFlags,
                              uint32_t mipLevels);
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
  VkFormat FindDepthFormat();
  VkShaderModule CreateShaderModule(const std::vector<char>& code);
  void CreateImage(uint32_t width,
                   uint32_t height,
                   uint32_t mipLevels,
                   VkSampleCountFlagBits numSamples,
                   VkFormat format,
                   VkImageTiling tiling,
                   VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties,
                   VkImage& image,
                   VkDeviceMemory& imageMemory);
  uint32_t FindMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);
  void CreateBuffer(VkDeviceSize size,
                    VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties,
                    VkBuffer& buffer,
                    VkDeviceMemory& bufferMemory);
  void TransitionImageLayout(VkImage image,
                             VkFormat format,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout,
                             uint32_t mipLevels);
  void CopyBufferToImage(VkBuffer buffer,
                         VkImage image,
                         uint32_t width,
                         uint32_t height);
  VkCommandBuffer BeginSingleTimeCommands();
  void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
  void GenerateMipmaps(VkImage image,
                       VkFormat imageFormat,
                       int32_t texWidth,
                       int32_t texHeight,
                       uint32_t mipLevels);
  void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void RecreateSwapChain();
  void CleanupSwapChain();
  void UpdateUniformBuffer(uint32_t currentImage);
  void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

private:
  Window* mWindow = nullptr;
  VkInstance mInstance;
  VkDebugUtilsMessengerEXT mDebugMessenger;
  VkSurfaceKHR mSurface;
  VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
  VkSampleCountFlagBits mMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
  VkDevice mDevice;
  VkQueue mGraphicsQueue;
  VkQueue mPresentQueue;
  VkSwapchainKHR mSwapChain;
  std::vector<VkImage> mSwapChainImages;
  VkFormat mSwapChainImageFormat;
  VkExtent2D mSwapChainExtent;
  std::vector<VkImageView> mSwapChainImageViews;
  std::vector<VkFramebuffer> mSwapChainFramebuffers;
  VkRenderPass mRenderPass;
  VkDescriptorSetLayout mDescriptorSetLayout;
  VkPipelineLayout mPipelineLayout;
  VkPipeline mGraphicsPipeline;

  VkCommandPool mCommandPool;

  VkImage mColorImage;
  VkDeviceMemory mColorImageMemory;
  VkImageView mColorImageView;

  VkImage mDepthImage;
  VkDeviceMemory mDepthImageMemory;
  VkImageView mDepthImageView;

  uint32_t mMipLevels;
  VkImage mTextureImage;
  VkDeviceMemory mTextureImageMemory;
  VkImageView mTextureImageView;
  VkSampler mTextureSampler;

  std::vector<Vertex> mVertices;
  std::vector<uint32_t> mIndices;
  VkBuffer mVertexBuffer;
  VkDeviceMemory mVertexBufferMemory;
  VkBuffer mIndexBuffer;
  VkDeviceMemory mIndexBufferMemory;

  std::vector<VkBuffer> mUniformBuffers;
  std::vector<VkDeviceMemory> mUniformBuffersMemory;
  std::vector<void*> mUniformBuffersMapped;

  VkDescriptorPool mDescriptorPool;
  std::vector<VkDescriptorSet> mDescriptorSets;

  std::vector<VkCommandBuffer> mCommandBuffers;

  std::vector<VkSemaphore> mImageAvailableSemaphores;
  std::vector<VkSemaphore> mRenderFinishedSemaphores;
  std::vector<VkFence> mInFlightFences;
  uint32_t mCurrentFrame = 0;

  bool mFramebufferResized = false;
};

}  // namespace hkr
