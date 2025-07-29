#pragma once

#include "Core/Math.h"
#include "Renderer/Image.h"
#include "Renderer/Buffer.h"

#include <vk_mem_alloc.h>
#include <volk.h>
#include <tiny_gltf.h>

namespace hkr {

struct glTFVertex {
  Vec3 position;
  Vec3 normal;
  Vec2 uv;
  Vec4 color = Vec4(1.0f);
  Vec4 joint0;
  Vec4 weight0;
  Vec4 tangent;
};

struct glTFSampler {
  VkSampler sampler;
};

struct glTFImage {
  Texture image;
};

struct glTFTexture {
  int imageIndex = -1;
  int samplerIndex = -1;
};

struct glTFMaterial {
  // enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
  // AlphaMode alphaMode = ALPHAMODE_OPAQUE;
  // float alphaCutoff = 1.0f;
  Vec4 baseColorFactor = Vec4(1.0f);
  Vec3 emissiveFactor = Vec3(0.0f);
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;
  // index of images vector in model class
  int baseColorTextureIndex = -1;
  int metallicRoughnessTextureIndex = -1;
  int normalTextureIndex = -1;
  int occlusionTextureIndex = -1;
  int emissiveTextureIndex = -1;

  VkDescriptorSetLayout materialSetLayout;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

struct glTFPrimitive {
  uint32_t firstVertex;
  uint32_t vertexCount;
  uint32_t firstIndex;
  uint32_t indexCount;
  int materialIndex = -1;

  struct Extent {
    Vec3 min = Vec3(FLT_MAX);
    Vec3 max = Vec3(-FLT_MAX);
    Vec3 size = Vec3(0.0f);
    Vec3 center = Vec3(0.0f);
    float radius = 0.0f;
  } extent;

  void setDimensions(Vec3 min, Vec3 max);
};

struct glTFMesh {
  std::vector<glTFPrimitive> primitives;
};

struct glTFNode {
  // index of nodes vector in gltfModel class
  std::vector<uint32_t> childIndices;
  int meshIndex = -1;
  Vec3 translation = Vec3(0.0f);
  Vec3 scale = Vec3(1.0f);
  glm::quat rotation{};
  Mat4 localTransform = Mat4(1.0f);
  struct UniformData {
    Mat4 globalTransform = Mat4(1.0f);
  } uniformData;
  UniformBuffer ubo;
  VkDescriptorSet uboDescriptorSet;
};

class glTFModel {
public:
  void Load(VkDevice device,
            VkQueue queue,
            VkCommandPool commandPool,
            VmaAllocator allocator,
            const std::string& fileName,
            VkBufferUsageFlags2 bufferUsageFlags);
  void Draw();
  void Cleanup();

  // vertices and indices buffers for all primitives in all meshes
  Buffer vertices;
  Buffer indices;
  std::vector<glTFSampler> samplers;
  std::vector<glTFImage> images;
  std::vector<glTFTexture> textures;
  std::vector<glTFMaterial> materials;
  std::vector<glTFMesh> meshes;
  // all nodes for all scenes
  std::vector<glTFNode> nodes;
  // node indices in default scene
  std::vector<uint32_t> nodeIndices;
  // node indices in default scene with no parent
  std::vector<uint32_t> topLevelNodeIndices;

private:
  void LoadSamplers(const tinygltf::Model& model);
  void LoadImages(const tinygltf::Model& model);
  void LoadTextures(const tinygltf::Model& model);
  void LoadMaterials(const tinygltf::Model& model);
  void LoadMeshes(const tinygltf::Model& model);
  void LoadNodes(const tinygltf::Model& model);
  void LoadScene(const tinygltf::Model& model);
  void CreateDescriptorSets();
  void UpdateNodes(uint32_t parentIndex, uint32_t index);

private:
  VkDevice mDevice;
  VkQueue mQueue;
  VkCommandPool mCommandPool;
  VmaAllocator mAllocator;
  std::string mFilePath;

  // usage flags for vertices and indices buffers
  VkBufferUsageFlags2 mBufferUsageFlags;
  VkDescriptorPool descritorPool;
  VkDescriptorSetLayout uboSetLayout;
};

}  // namespace hkr
