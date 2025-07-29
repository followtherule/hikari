#pragma once

#include "Renderer/Buffer.h"
#include "Renderer/Image.h"
#include "Renderer/Common.h"
#include "Renderer/Model.h"
#include "Renderer/Skybox.h"

#include <volk.h>

#include <vector>

namespace hkr {

struct AccelerationStructure {
  VkAccelerationStructureKHR AS;
  Buffer buffer;
  VkDeviceAddress deviceAddress;
};

struct GeometryNode {
  VkDeviceAddress vertexBufferDeviceAddr;
  VkDeviceAddress indexBufferDeviceAddr;
  int32_t BaseColorTextureIndex = -1;
  int32_t OcclusionTextureIndex = -1;
  int32_t NormalTextureIndex = -1;
};

class Raytracer {
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

private:
  void BuildBLAS();
  void BuildTLAS();
  void CreateShaderBindingTables();

  void CreateStorageImage();
  void CreatePipelineLayout();
  void CreatePipeline();
  void CreatePipelineCache();

  void CreateDescriptorPool();
  void CreateDescriptorSetLayout();
  void CreateDescriptorSets();

private:
  // rendering context
  VkDevice mDevice;
  VkPhysicalDevice mPhysDevice;
  VkQueue mGraphicsQueue;
  VmaAllocator mAllocator;
  VkFormat mSwapchainImageFormat;
  VkCommandPool mCommandPool;
  int mWidth = 0;
  int mHeight = 0;
  glTFModel* mModel = nullptr;
  Skybox* mSkybox = nullptr;
  std::string mAssetPath;
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> mUniformBuffers;

  // raytracing
  Image mStorageImage;
  VkDeviceSize mHandleSize;
  VkDeviceSize mHandleAlignment;
  VkDeviceSize mBaseAlignment;
  VkDeviceSize mStride;
  Buffer mGeometryNodeBuffer;
  VkDeviceAddress mVertexBufferDeviceAddress;
  VkDeviceAddress mIndexBufferDeviceAddress;
  AccelerationStructure mBLAS;
  AccelerationStructure mTLAS;
  MappableBuffer mRaygenShaderBindingTable;
  VkStridedDeviceAddressRegionKHR mRaygenSBTAddr{};
  MappableBuffer mMissShaderBindingTable;
  VkStridedDeviceAddressRegionKHR mMissSBTAddr{};
  MappableBuffer mHitShaderBindingTable;
  VkStridedDeviceAddressRegionKHR mHitSBTAddr{};

  VkDescriptorSetLayout mDescriptorSetLayout;
  VkDescriptorPool mDescriptorPool;
  std::vector<VkDescriptorSet> mDescriptorSets;

  VkPipelineCache mPipelineCache{VK_NULL_HANDLE};
  VkPipelineLayout mPipelineLayout;
  VkPipeline mRaytracingPipeline;
};

}  // namespace hkr
