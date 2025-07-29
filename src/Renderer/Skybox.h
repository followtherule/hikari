#pragma once

#include "Renderer/Common.h"
#include "Renderer/Cube.h"
#include "Renderer/Image.h"

#include <volk.h>

#include <vector>

namespace hkr {

class Skybox {
public:
  void Create(
      VkDevice device,
      VkQueue queue,
      VkCommandPool commandPool,
      const std::array<UniformBuffer, MAX_FRAMES_IN_FLIGHT>& uniformBuffers,
      VmaAllocator allocator,
      const std::string& assetPath,
      const std::string& cubemapRelPath,
      VkBufferUsageFlags2 bufferUsageFlags);
  void Draw(VkCommandBuffer commandBuffer, uint32_t currentFrame);
  void Cleanup(VmaAllocator allocator);

  Cubemap cubemap;
  VkSampler cubemapSampler;

private:
  void CreateDescriptorPool();
  void CreateDescriptorSetLayout();
  void CreateDescriptorSets();
  void CreatePipelineLayout();
  void CreatePipeline();

private:
  Cube mCube;
  VkDevice mDevice;
  VkDescriptorPool mDescriptorPool;
  VkDescriptorSetLayout mDescriptorSetLayout;
  std::vector<VkDescriptorSet> mDescriptorSets;
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> mUniformBuffers;
  VkPipelineLayout mPipelineLayout;
  VkPipeline mPipeline;
  std::string mAssetPath;
};

}  // namespace hkr
