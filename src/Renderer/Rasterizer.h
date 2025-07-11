#pragma once

#include "Renderer/Image.h"
#include "Renderer/Buffer.h"
#include "Renderer/Common.h"

#include "Core/Math.h"

#include <volk.h>

#include <vector>
#include <string>

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

class Rasterizer {
public:
  void Init(
      VkDevice device,
      VkPhysicalDevice physDevice,
      VkQueue queue,
      VkCommandPool commandPool,
      const std::array<UniformBuffer, MAX_FRAMES_IN_FLIGHT>& uniformBuffers,
      VmaAllocator allocator,
      VkFormat swapchainImageFormat,
      int width,
      int height,
      const std::string& modelPath,
      const std::string& texturePath,
      const std::string& shaderPath);
  void OnResize(int width, int height);
  void Cleanup();
  void RecordCommandBuffer(VkCommandBuffer commandBuffer,
                           uint32_t currentFrame,
                           VkImage swapchainImage);

private:
  void CreateDescriptorSetLayout();
  void CreateGraphicsPipeline();
  void CreatePipelineCache();

  void CreateTextureImage();
  void CreateTextureSampler();
  void LoadModel();
  void CreateVertexBuffer();
  void CreateIndexBuffer();
  void CreateUniformBuffers();
  void CreateDescriptorPool();
  void CreateDescriptorSets();
  void CreateSyncObjects();
  // VkSampleCountFlagBits GetMaxUsableSampleCount();
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

  VkFormat FindDepthFormat();
  VkShaderModule CreateShaderModule(const std::vector<char>& code);
  void UpdateUniformBuffer(uint32_t currentImage);

private:
  VkDevice mDevice;
  VkPhysicalDevice mPhysDevice;
  VkQueue mGraphicsQueue;
  VmaAllocator mAllocator;
  VkFormat mSwapchainImageFormat;
  VkCommandPool mCommandPool;
  int mWidth = 0;
  int mHeight = 0;

  // off-screen images for the first renderpass
  Image mColorImage;
  Image mDepthImage;
  bool mRequireStencil = false;

  // VkCommandPool mCommandPool;
  // std::vector<VkCommandBuffer> mCommandBuffers;
  //
  // // descriptor resources
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> mUniformBuffers;

  std::string mModelPath;
  std::string mTexturePath;
  std::string mShaderPath;
  Texture mTextureImage;
  VkSampler mTextureSampler;
  std::vector<Vertex> mVertices;
  std::vector<uint32_t> mIndices;
  Buffer mVertexBuffer;
  Buffer mIndexBuffer;

  VkDescriptorSetLayout mDescriptorSetLayout;
  VkDescriptorPool mDescriptorPool;
  std::vector<VkDescriptorSet> mDescriptorSets;

  VkPipelineCache mPipelineCache{VK_NULL_HANDLE};
  VkPipelineLayout mPipelineLayout;
  VkPipeline mGraphicsPipeline;
};

}  // namespace hkr
