#pragma once

#include "Renderer/Image.h"
#include "Renderer/Buffer.h"
#include "Renderer/Common.h"
#include "Renderer/Skybox.h"
#include "Renderer/Model.h"

#include <volk.h>

#include <cstdint>
#include <vector>
#include <string>

namespace hkr {

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
      glTFModel* model,
      Skybox* skybox,
      const std::string& assetPath);
  void OnResize(int width, int height);
  void Cleanup();
  void RecordCommandBuffer(VkCommandBuffer commandBuffer,
                           uint32_t currentFrame,
                           VkImage swapchainImage);
  void DrawNode(VkCommandBuffer commandBuffer,
                uint32_t currentFrame,
                const glTFNode& node);
  void Draw(VkCommandBuffer commandBuffer, uint32_t currentFrame);

private:
  void CreateAttachmentImage();
  void CreatePipelineCache();
  void CreatePipelineLayout();
  void CreatePipeline();

  void CreateUniformBuffers();
  void CreateDescriptorPool();
  void CreateDescriptorSetLayout();
  void CreateDescriptorSets();
  // VkSampleCountFlagBits GetMaxUsableSampleCount();
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
  VkFormat FindDepthFormat();

private:
  VkDevice mDevice;
  VkPhysicalDevice mPhysDevice;
  VkQueue mGraphicsQueue;
  VmaAllocator mAllocator;
  VkFormat mSwapchainImageFormat;
  VkCommandPool mCommandPool;
  int mWidth = 0;
  int mHeight = 0;
  std::string mModelPath;
  std::string mTexturePath;
  std::string mAssetPath;
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> mUniformBuffers;

  // off-screen images for the first dynamic rendering
  Image mColorImage;
  Image mDepthImage;
  bool mRequireStencil = false;

  glTFModel* mModel = nullptr;
  Skybox* mSkybox = nullptr;

  VkDescriptorPool mDescriptorPool;
  VkDescriptorSetLayout mUboDescriptorSetLayout;
  VkDescriptorSetLayout mImageDescriptorSetLayout;
  std::vector<VkDescriptorSet> mUboDescriptorSets;
  std::vector<std::vector<VkDescriptorSet>> mImageDescriptorSets;

  VkPipelineCache mPipelineCache{VK_NULL_HANDLE};
  VkPipelineLayout mPipelineLayout;
  VkPipeline mGraphicsPipeline;
};

}  // namespace hkr
