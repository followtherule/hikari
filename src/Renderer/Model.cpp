#include "Renderer/Model.h"
#include "Renderer/Buffer.h"
#include "Renderer/Image.h"
#include "Renderer/Descriptor.h"
#include "Util/vk_debug.h"
#include "Util/Assert.h"
#include "Util/vk_util.h"
#include "Util/Filesystem.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cstdint>

namespace {

VkFilter glTFFilterToVkFilter(int glTFFilter) {
  switch (glTFFilter) {
    case TINYGLTF_TEXTURE_FILTER_LINEAR:
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: {
      return VK_FILTER_LINEAR;
    }
    case TINYGLTF_TEXTURE_FILTER_NEAREST:
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: {
      return VK_FILTER_NEAREST;
    }
    default:
      return VK_FILTER_NEAREST;
  }
}

VkSamplerMipmapMode glTFMipmapToVkMipmap(int glTFMipmap) {
  switch (glTFMipmap) {
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: {
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: {
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
    default:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }
}

VkSamplerAddressMode glTFAddressModeToVkAddressMode(int glTFAddressMode) {
  switch (glTFAddressMode) {
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    default:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
}

}  // namespace

namespace hkr {

void glTFModel::Load(VkDevice device,
                     VkQueue queue,
                     VkCommandPool commandPool,
                     VmaAllocator allocator,
                     const std::string& fileName,
                     VkBufferUsageFlags2 bufferUsageFlags) {
  mDevice = device;
  mQueue = queue;
  mCommandPool = commandPool;
  mAllocator = allocator;
  mBufferUsageFlags = bufferUsageFlags;
  HKR_INFO("Loading model: {}", fileName.c_str());
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  loader.SetImageLoader(
      [](tinygltf::Image* image, const int imageIndex, std::string* err,
         std::string* warn, int req_width, int req_height,
         const unsigned char* bytes, int size, void* userData) -> bool {
        if (!image->uri.empty() && GetFileExtension(image->uri) == "ktx2") {
          return true;
        }
        return tinygltf::LoadImageData(image, imageIndex, err, warn, req_width,
                                       req_height, bytes, size, userData);
      },
      nullptr);
  std::string err;
  std::string warn;
  bool result = false;
  auto fileExtension = GetFileExtension(fileName);
  if (fileExtension == "gltf") {
    result = loader.LoadASCIIFromFile(&model, &err, &warn, fileName);
  } else if (fileExtension == "glb") {
    result = loader.LoadBinaryFromFile(&model, &err, &warn, fileName);
  } else {
    HKR_ASSERT(0);
  }
  if (!warn.empty()) {
    HKR_WARN(warn.c_str());
  }
  if (!err.empty()) {
    HKR_ERROR(err.c_str());
  }
  HKR_ASSERT(result);
  mFilePath = GetFilePath(fileName);

  // samplers
  LoadSamplers(model);

  // images
  LoadImages(model);

  // textures
  LoadTextures(model);

  // materials
  LoadMaterials(model);

  // meshes
  LoadMeshes(model);

  // nodes
  LoadNodes(model);

  // default scene
  LoadScene(model);

  // descriptor sets
  // CreateDescriptorSets();
}

void glTFModel::LoadSamplers(const tinygltf::Model& model) {
  const size_t samplerCount = model.samplers.size();
  samplers.resize(samplerCount);
  for (size_t i = 0; i < samplerCount; i++) {
    const auto& sampler = model.samplers[i];
    glTFSampler& newSampler = samplers[i];
    SamplerBuilder builder;
    builder.SetMinFilter(glTFFilterToVkFilter(sampler.minFilter))
        .SetMagFilter(glTFFilterToVkFilter(sampler.magFilter))
        .SetMipmapMode(glTFMipmapToVkMipmap(sampler.magFilter))
        .SetAddressModeU(glTFAddressModeToVkAddressMode(sampler.wrapS))
        .SetAddressModeV(glTFAddressModeToVkAddressMode(sampler.wrapT))
        .SetMaxAnisotropy(8.0f);
    newSampler.sampler = builder.Build(mDevice);
  }
  // default sampler
  glTFSampler& newSampler = samplers.emplace_back();
  SamplerBuilder builder;
  builder.SetMaxAnisotropy(8.0f);
  newSampler.sampler = builder.Build(mDevice);
}

static void CreateDefaultImage(VkDevice device,
                               VkQueue queue,
                               VkCommandPool commandPool,
                               VmaAllocator allocator,
                               glTFImage& defaultImage) {
  defaultImage.image.Create(device, allocator, 1, 1, 1,
                            VK_FORMAT_R8G8B8A8_UNORM);
  std::array<uint8_t, 4> defaultImageData{255, 255, 255, 255};
  uint32_t dataSize = defaultImageData.size() * sizeof(uint8_t);
  StagingBuffer staging;
  staging.Create(allocator, dataSize);
  staging.Map(allocator);
  staging.Write(defaultImageData.data(), dataSize);
  staging.Unmap(allocator);
  VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, commandPool);
  TransitImageLayout(commandBuffer, defaultImage.image.image,
                     VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
  CopyBufferToImage(commandBuffer, staging.buffer, defaultImage.image.image, 1,
                    1);
  TransitImageLayout(commandBuffer, defaultImage.image.image,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
  EndOneTimeCommands(device, queue, commandPool, commandBuffer);
  staging.Cleanup(allocator);
}

// create image in gpu and generate mipmap
void glTFModel::LoadImages(const tinygltf::Model& model) {
  const size_t imageCount = model.images.size();
  images.resize(imageCount);
  for (size_t i = 0; i < imageCount; i++) {
    const auto& image = model.images[i];
    glTFImage& newImage = images[i];

    // if (image.uri.empty() || image.width == 0 || image.height == 0 ||
    //     image.component == -1) {
    //   CreateDefaultImage(mDevice, mQueue, mCommandPool, mAllocator,
    //   newImage); continue;
    // }

    if (!image.uri.empty() && GetFileExtension(image.uri) == "ktx2") {
      const std::string fileName = mFilePath + "/" + image.uri;
      newImage.image.Load(mDevice, mAllocator, mQueue, mCommandPool, fileName);
    } else {
      const int width = image.width;
      const int height = image.height;
      std::vector<unsigned char> imageData;
      // const unsigned char* imageData = nullptr;
      imageData.resize(width * height * 4);
      if (image.component == 4) {
        imageData = image.image;
        HKR_ASSERT(imageData.size() * sizeof(unsigned char) ==
                   width * height * 4);
      } else {
        for (size_t j = 0; j < width * height; j++) {
          for (size_t k = 0; k < image.component; k++) {
            imageData[j * 4 + k] = image.image[j * image.component + k];
          }
        }
      }
      // upload image data to gpu image
      StagingBuffer staging;
      const uint32_t imageSize = imageData.size() * sizeof(unsigned char);
      staging.Create(mAllocator, imageSize);
      staging.Map(mAllocator);
      staging.Write(imageData.data(), imageSize);
      staging.Unmap(mAllocator);
      uint32_t mipLevels = GetMipLevels(width, height);
      newImage.image.Create(mDevice, mAllocator, width, height, mipLevels,
                            VK_FORMAT_R8G8B8A8_UNORM);
      // VK_FORMAT_R8G8B8A8_SRGB);

      VkCommandBuffer commandBuffer =
          BeginOneTimeCommands(mDevice, mCommandPool);
      TransitImageLayout(commandBuffer, newImage.image.image,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
      CopyBufferToImage(commandBuffer, staging.buffer, newImage.image.image,
                        static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height));
      // transit to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while
      // generating mipmaps
      GenerateMipmaps(commandBuffer, newImage.image.image, width, height,
                      mipLevels);
      EndOneTimeCommands(mDevice, mQueue, mCommandPool, commandBuffer);
      staging.Cleanup(mAllocator);
    }
  }
  // create default image
  auto& defaultImage = images.emplace_back();
  CreateDefaultImage(mDevice, mQueue, mCommandPool, mAllocator, defaultImage);
}

void glTFModel::LoadTextures(const tinygltf::Model& model) {
  const size_t texCount = model.textures.size();
  textures.resize(texCount);
  for (size_t i = 0; i < texCount; i++) {
    const auto& tex = model.textures[i];
    glTFTexture& newTex = textures[i];
    newTex.imageIndex = tex.source == -1 ? images.size() - 1 : tex.source;
    newTex.samplerIndex = tex.sampler == -1 ? samplers.size() - 1 : tex.sampler;
  }
  glTFTexture& defaultTex = textures.emplace_back();
  defaultTex.imageIndex = images.size() - 1;
  defaultTex.samplerIndex = samplers.size() - 1;
}

void glTFModel::LoadMaterials(const tinygltf::Model& model) {
  const size_t matCount = model.materials.size();
  materials.resize(matCount);
  const uint32_t defaultTextureIndex =
      static_cast<uint32_t>(textures.size()) - 1;
  for (size_t i = 0; i < matCount; i++) {
    const tinygltf::Material& mat = model.materials[i];
    glTFMaterial& material = materials[i];
    const auto& metallicRoughness = mat.pbrMetallicRoughness;
    // base color texture
    material.baseColorTextureIndex = metallicRoughness.baseColorTexture.index;
    // base color factor
    auto& colorFactor = metallicRoughness.baseColorFactor;
    material.baseColorFactor = {colorFactor[0], colorFactor[1], colorFactor[2],
                                colorFactor[3]};
    // metallicRoughness texture
    material.metallicRoughnessTextureIndex =
        metallicRoughness.metallicRoughnessTexture.index;
    // metallic factor
    material.metallicFactor = metallicRoughness.metallicFactor;
    // roughness factor
    material.roughnessFactor = metallicRoughness.roughnessFactor;
    // normal texture
    material.normalTextureIndex = mat.normalTexture.index;
    // occlusion texture
    material.occlusionTextureIndex = mat.occlusionTexture.index;
    // emissive texture
    material.emissiveTextureIndex = mat.emissiveTexture.index;
    // emissive factor
    auto& emissiveFactor = mat.emissiveFactor;
    material.emissiveFactor = {emissiveFactor[0], emissiveFactor[1],
                               emissiveFactor[2]};
  }
  // default material
  auto& defaultMaterial = materials.emplace_back();
  defaultMaterial.baseColorTextureIndex = defaultTextureIndex;
  defaultMaterial.metallicRoughnessTextureIndex = defaultTextureIndex;
  defaultMaterial.normalTextureIndex = defaultTextureIndex;
  defaultMaterial.occlusionTextureIndex = defaultTextureIndex;
  defaultMaterial.emissiveTextureIndex = defaultTextureIndex;
}

void glTFModel::LoadMeshes(const tinygltf::Model& model) {
  const size_t meshCount = model.meshes.size();
  meshes.resize(meshCount);
  // vertex and index data for all primitives in all meshes
  std::vector<glTFVertex> vertexData;
  std::vector<uint32_t> indexData;
  for (size_t i = 0; i < meshCount; i++) {
    const auto& mesh = model.meshes[i];
    glTFMesh& newMesh = meshes[i];
    for (const tinygltf::Primitive& prim : mesh.primitives) {
      if (prim.indices < 0) {
        continue;
      }
      glTFPrimitive& newPrim = newMesh.primitives.emplace_back();
      size_t firstVertex = vertexData.size();
      uint32_t vertexCount = 0;
      size_t firstIndex = indexData.size();
      uint32_t indexCount = 0;
      // indices
      {
        const tinygltf::Accessor& accessor = model.accessors[prim.indices];
        indexCount = static_cast<uint32_t>(accessor.count);
        const tinygltf::BufferView& bufferView =
            model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        const unsigned char* pIndex =
            &buffer.data[accessor.byteOffset + bufferView.byteOffset];
        const uint32_t indexStride = accessor.ByteStride(bufferView);

        switch (accessor.componentType) {
          case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            for (size_t index = 0; index < indexCount; index++) {
              const uint32_t* idx = reinterpret_cast<const uint32_t*>(
                  pIndex + index * indexStride);
              indexData.push_back(*idx + firstVertex);
            }
            break;
          }
          case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            for (size_t index = 0; index < indexCount; index++) {
              const uint16_t* idx = reinterpret_cast<const uint16_t*>(
                  pIndex + index * indexStride);
              indexData.push_back(*idx + firstVertex);
            }
            break;
          }
          case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            for (size_t index = 0; index < indexCount; index++) {
              const uint8_t* idx = reinterpret_cast<const uint8_t*>(
                  pIndex + index * indexStride);
              indexData.push_back(*idx + firstVertex);
            }
            break;
          }
          default:
            HKR_ASSERT(0);
        }
      }

      // POSITION
      const unsigned char* pPos = nullptr;
      uint32_t posStride = 0;
      {
        HKR_ASSERT(prim.attributes.find("POSITION") != prim.attributes.end());
        const tinygltf::Accessor& posAccessor =
            model.accessors[prim.attributes.find("POSITION")->second];
        vertexCount = posAccessor.count;
        const tinygltf::BufferView& posView =
            model.bufferViews[posAccessor.bufferView];
        pPos = reinterpret_cast<const unsigned char*>(
            &model.buffers[posView.buffer]
                 .data[posAccessor.byteOffset + posView.byteOffset]);
        posStride = posAccessor.ByteStride(posView);
      }
      // NORMAL
      const unsigned char* pNormal = nullptr;
      uint32_t normalStride = 0;
      {
        if (prim.attributes.find("NORMAL") != prim.attributes.end()) {
          const tinygltf::Accessor& normalAccessor =
              model.accessors[prim.attributes.find("NORMAL")->second];
          const tinygltf::BufferView& normalView =
              model.bufferViews[normalAccessor.bufferView];
          pNormal = reinterpret_cast<const unsigned char*>(
              &model.buffers[normalView.buffer]
                   .data[normalAccessor.byteOffset + normalView.byteOffset]);
          normalStride = normalAccessor.ByteStride(normalView);
        }
      }
      // TEXCOORD_0
      const unsigned char* pTexCoord = nullptr;
      uint32_t texCoordStride = 0;
      {
        if (prim.attributes.find("TEXCOORD_0") != prim.attributes.end()) {
          const tinygltf::Accessor& texAccessor =
              model.accessors[prim.attributes.find("TEXCOORD_0")->second];
          const tinygltf::BufferView& texView =
              model.bufferViews[texAccessor.bufferView];
          pTexCoord = reinterpret_cast<const unsigned char*>(
              &model.buffers[texView.buffer]
                   .data[texAccessor.byteOffset + texView.byteOffset]);
          texCoordStride = texAccessor.ByteStride(texView);
        }
      }
      // COLOR_0
      const unsigned char* pColor = nullptr;
      uint32_t colorStride = 0;
      uint32_t colorComponents = 0;
      int colorComponentType = -1;
      {
        if (prim.attributes.find("COLOR_0") != prim.attributes.end()) {
          const tinygltf::Accessor& colorAccessor =
              model.accessors[prim.attributes.find("COLOR_0")->second];
          const tinygltf::BufferView& colorView =
              model.bufferViews[colorAccessor.bufferView];
          pColor = reinterpret_cast<const unsigned char*>(
              &model.buffers[colorView.buffer]
                   .data[colorAccessor.byteOffset + colorView.byteOffset]);
          colorStride = colorAccessor.ByteStride(colorView);
          colorComponentType = colorAccessor.componentType;
          HKR_ASSERT(colorAccessor.type == TINYGLTF_TYPE_VEC3 ||
                     colorAccessor.type == TINYGLTF_TYPE_VEC4);
          colorComponents = colorAccessor.type == TINYGLTF_TYPE_VEC3 ? 3 : 4;
        }
      }
      // TANGENT
      const unsigned char* pTangent = nullptr;
      uint32_t tangentStride = 0;
      {
        if (prim.attributes.find("TANGENT") != prim.attributes.end()) {
          const tinygltf::Accessor& tangentAccessor =
              model.accessors[prim.attributes.find("TANGENT")->second];
          const tinygltf::BufferView& tangentView =
              model.bufferViews[tangentAccessor.bufferView];
          pTangent = reinterpret_cast<const unsigned char*>(
              &model.buffers[tangentView.buffer]
                   .data[tangentAccessor.byteOffset + tangentView.byteOffset]);
          tangentStride = tangentAccessor.ByteStride(tangentView);
        }
      }
      vertexData.resize(firstVertex + vertexCount);
      for (size_t v = 0; v < vertexCount; v++) {
        glTFVertex& vertex = vertexData[firstVertex + v];
#define RCASTF(...) reinterpret_cast<const float*>(__VA_ARGS__)
#define RCASTUS(...) reinterpret_cast<const unsigned short*>(__VA_ARGS__)
#define RCASTUI(...) reinterpret_cast<const unsigned int*>(__VA_ARGS__)
        // position
        vertex.position =
            Vec4(glm::make_vec3(RCASTF(&pPos[v * posStride])), 1.0f);
        // normal
        vertex.normal = pNormal ? glm::normalize(glm::make_vec3(
                                      RCASTF(&pNormal[v * normalStride])))
                                : Vec3(0.0f);
        // vertex.position.y *= -1.0f;
        // vertex.normal.y *= -1.0f;
        // uv
        vertex.uv = pTexCoord
                        ? glm::make_vec2(RCASTF(&pTexCoord[v * texCoordStride]))
                        : Vec2(0.0f);
        // color
        if (pColor) {
          switch (colorComponentType) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
              if (colorComponents == 3) {
                vertex.color = Vec4(
                    glm::make_vec3(RCASTF(&pColor[v * colorStride])), 1.0f);
              } else {
                vertex.color = glm::make_vec4(RCASTF(&pColor[v * colorStride]));
              }
              break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
              if (colorComponents == 3) {
                vertex.color = Vec4(
                    glm::make_vec3(RCASTUS(&pColor[v * colorStride])), 1.0f);
              } else {
                vertex.color =
                    glm::make_vec4(RCASTUS(&pColor[v * colorStride]));
              }
              break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
              if (colorComponents == 3) {
                vertex.color = Vec4(
                    glm::make_vec3(RCASTUI(&pColor[v * colorStride])), 1.0f);
              } else {
                vertex.color =
                    glm::make_vec4(RCASTUI(&pColor[v * colorStride]));
              }
              break;
          }
        }
        // tangent
        vertex.tangent =
            pTangent ? glm::make_vec4(RCASTF(&pTangent[v * tangentStride]))
                     : Vec4(0.0f);
#undef RCASTF
#undef RCASTUS
#undef RCASTUI
      }
      newPrim.firstVertex = firstVertex;
      newPrim.vertexCount = vertexCount;
      newPrim.firstIndex = firstIndex;
      newPrim.indexCount = indexCount;
      newPrim.materialIndex =
          prim.material > -1 ? prim.material : materials.size() - 1;
    }
  }
  size_t vertexDataSize = vertexData.size() * sizeof(glTFVertex);
  size_t indexDataSize = indexData.size() * sizeof(uint32_t);

  // upload vertex/index data to gpu buffer
  StagingBuffer vertexStaging;
  vertexStaging.Create(mAllocator, vertexDataSize);
  vertexStaging.Map(mAllocator);
  vertexStaging.Write(vertexData.data(), vertexDataSize);
  vertexStaging.Unmap(mAllocator);
  vertices.Create(mAllocator, vertexDataSize,
                  VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | mBufferUsageFlags);
  StagingBuffer indexStaging;
  indexStaging.Create(mAllocator, indexDataSize);
  indexStaging.Map(mAllocator);
  indexStaging.Write(indexData.data(), indexDataSize);
  indexStaging.Unmap(mAllocator);
  indices.Create(mAllocator, indexDataSize,
                 VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | mBufferUsageFlags);

  VkCommandBuffer commandBuffer = BeginOneTimeCommands(mDevice, mCommandPool);
  CopyBufferToBuffer(commandBuffer, vertexStaging.buffer, vertices.buffer,
                     vertexDataSize);
  CopyBufferToBuffer(commandBuffer, indexStaging.buffer, indices.buffer,
                     indexDataSize);
  EndOneTimeCommands(mDevice, mQueue, mCommandPool, commandBuffer);
  vertexStaging.Cleanup(mAllocator);
  indexStaging.Cleanup(mAllocator);
}

void glTFModel::LoadNodes(const tinygltf::Model& model) {
  const size_t nodeCount = model.nodes.size();
  nodes.resize(nodeCount);
  for (size_t i = 0; i < nodeCount; i++) {
    const auto& node = model.nodes[i];
    glTFNode& newNode = nodes[i];
    // local transform
    if (node.translation.size() == 3) {
      newNode.translation = glm::make_vec3(node.translation.data());
    }
    if (node.rotation.size() == 4) {
      newNode.rotation = glm::make_quat(node.rotation.data());
    }
    if (node.scale.size() == 3) {
      newNode.scale = glm::make_vec3(node.scale.data());
    }
    newNode.localTransform = glm::translate(Mat4(1.0f), newNode.translation) *
                             glm::mat4(newNode.rotation) *
                             glm::scale(Mat4(1.0f), newNode.scale);
    if (node.matrix.size() == 16) {
      newNode.localTransform = glm::make_mat4x4(node.matrix.data());
    }

    newNode.ubo.Create(mAllocator, sizeof(glTFNode::UniformData));
    newNode.ubo.Map(mAllocator);

    // node has a mesh
    if (node.mesh > -1) {
      newNode.meshIndex = node.mesh;
    }
    // child
    for (int child : node.children) {
      newNode.childIndices.push_back(child);
    }
  }
}

void glTFModel::LoadScene(const tinygltf::Model& model) {
  uint32_t sceneIndex = model.defaultScene > -1 ? model.defaultScene : 0;
  const tinygltf::Scene& scene = model.scenes[sceneIndex];
  const size_t nodeCount = scene.nodes.size();
  // get top level node indices in default scene
  topLevelNodeIndices.resize(nodeCount);
  for (size_t i = 0; i < nodeCount; i++) {
    topLevelNodeIndices[i] = scene.nodes[i];
    UpdateNodes(-1, topLevelNodeIndices[i]);
  }

  // get all node indices in default scene
  nodeIndices = topLevelNodeIndices;
  for (size_t i = 0; i < nodeIndices.size(); i++) {
    for (int nodeIndex : nodes[nodeIndices[i]].childIndices) {
      nodeIndices.push_back(nodeIndex);
    }
  }
}

void glTFModel::UpdateNodes(uint32_t parentIndex, uint32_t index) {
  Mat4 transform = parentIndex == -1
                       ? Mat4(1.0f)
                       : nodes[parentIndex].uniformData.globalTransform;
  nodes[index].uniformData.globalTransform =
      transform * nodes[index].localTransform;
  nodes[index].ubo.Write(&nodes[index].uniformData,
                         sizeof(glTFNode::UniformData));
  for (uint32_t childIndex : nodes[index].childIndices) {
    UpdateNodes(index, childIndex);
  }
}

void glTFModel::CreateDescriptorSets() {
  // uniform buffers for transforms of the nodes
  uint32_t uboCount = nodes.size();
  // combined image samplers for color/normal images
  uint32_t imageCount = images.size();
  std::vector<VkDescriptorPoolSize> poolSizes{
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboCount},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount},
  };
  // create descriptorPool
  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = uboCount + imageCount;
  VK_CHECK(vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &descritorPool));

  DescriptorSetLayoutBuilder uboLayoutBuilder(1);
  uboLayoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                              VK_SHADER_STAGE_VERTEX_BIT);
  uboSetLayout = uboLayoutBuilder.Build(mDevice);
  auto writeNode = [this](auto&& self, int nodeIndex) -> void {
    auto& node = nodes[nodeIndex];
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descritorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &uboSetLayout;
    VK_CHECK(
        vkAllocateDescriptorSets(mDevice, &allocInfo, &node.uboDescriptorSet));
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = node.ubo.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(glTFNode::UniformData);
    DescriptorSetWriter writer(1);
    writer.Write(node.uboDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 &bufferInfo);
    writer.Update(mDevice);
    for (int childIndex : node.childIndices) {
      self(self, childIndex);
    }
  };
  for (int nodeIndex : topLevelNodeIndices) {
    writeNode(writeNode, nodeIndex);
  }

  auto writeMaterial = [this](glTFMaterial& mat) {
    bool hasColor = mat.baseColorTextureIndex != -1;
    bool hasNormal = mat.normalTextureIndex != -1;
    bool hasMetallicRoughness = mat.metallicRoughnessTextureIndex != -1;
    bool hasOcclusion = mat.occlusionTextureIndex != -1;
    bool hasEmissive = mat.emissiveTextureIndex != -1;
    const uint32_t descriptorCount = hasColor + hasNormal +
                                     hasMetallicRoughness + hasOcclusion +
                                     hasEmissive;
    DescriptorSetLayoutBuilder builder(descriptorCount);
    uint32_t bindingCount = 0;
    if (hasColor) {
      builder.AddBinding(bindingCount,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                         VK_SHADER_STAGE_FRAGMENT_BIT);
      bindingCount++;
    }
    if (hasNormal) {
      builder.AddBinding(bindingCount,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                         VK_SHADER_STAGE_FRAGMENT_BIT);
      bindingCount++;
    }
    if (hasMetallicRoughness) {
      builder.AddBinding(bindingCount,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                         VK_SHADER_STAGE_FRAGMENT_BIT);
      bindingCount++;
    }
    if (hasColor) {
      builder.AddBinding(bindingCount,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                         VK_SHADER_STAGE_FRAGMENT_BIT);
      bindingCount++;
    }
    if (hasColor) {
      builder.AddBinding(bindingCount,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                         VK_SHADER_STAGE_FRAGMENT_BIT);
      bindingCount++;
    }
    mat.materialSetLayout = builder.Build(mDevice);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descritorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mat.materialSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(mDevice, &allocInfo, &mat.descriptorSet));

    DescriptorSetWriter writer(bindingCount);
    uint32_t binding = 0;
    if (hasColor) {
      auto& tex = textures[mat.baseColorTextureIndex];
      VkDescriptorImageInfo imageInfo{};
      imageInfo.sampler = tex.samplerIndex == -1
                              ? samplers.back().sampler
                              : samplers[tex.samplerIndex].sampler;
      imageInfo.imageView = tex.imageIndex == -1
                                ? images.back().image.imageView
                                : images[tex.imageIndex].image.imageView;
      // imageInfo.sampler = samplers[tex.samplerIndex].sampler;
      // imageInfo.imageView = images[tex.imageIndex].image.imageView;
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      writer.Write(mat.descriptorSet, binding,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
      binding++;
    }
    if (hasNormal) {
      auto& tex = textures[mat.normalTextureIndex];
      VkDescriptorImageInfo imageInfo{};
      imageInfo.sampler = tex.samplerIndex == -1
                              ? samplers.back().sampler
                              : samplers[tex.samplerIndex].sampler;
      imageInfo.imageView = tex.imageIndex == -1
                                ? images.back().image.imageView
                                : images[tex.imageIndex].image.imageView;
      // imageInfo.sampler = samplers[tex.samplerIndex].sampler;
      // imageInfo.imageView = images[tex.imageIndex].image.imageView;
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      writer.Write(mat.descriptorSet, binding,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
      binding++;
    }
    if (hasMetallicRoughness) {
      auto& tex = textures[mat.metallicRoughnessTextureIndex];
      VkDescriptorImageInfo imageInfo{};
      imageInfo.sampler = tex.samplerIndex == -1
                              ? samplers.back().sampler
                              : samplers[tex.samplerIndex].sampler;
      imageInfo.imageView = tex.imageIndex == -1
                                ? images.back().image.imageView
                                : images[tex.imageIndex].image.imageView;
      // imageInfo.sampler = samplers[tex.samplerIndex].sampler;
      // imageInfo.imageView = images[tex.imageIndex].image.imageView;
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      writer.Write(mat.descriptorSet, binding,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
      binding++;
    }
    if (hasOcclusion) {
      auto& tex = textures[mat.occlusionTextureIndex];
      VkDescriptorImageInfo imageInfo{};
      imageInfo.sampler = tex.samplerIndex == -1
                              ? samplers.back().sampler
                              : samplers[tex.samplerIndex].sampler;
      imageInfo.imageView = tex.imageIndex == -1
                                ? images.back().image.imageView
                                : images[tex.imageIndex].image.imageView;
      // imageInfo.sampler = samplers[tex.samplerIndex].sampler;
      // imageInfo.imageView = images[tex.imageIndex].image.imageView;
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      writer.Write(mat.descriptorSet, binding,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
      binding++;
    }
    if (hasEmissive) {
      auto& tex = textures[mat.emissiveTextureIndex];
      VkDescriptorImageInfo imageInfo{};
      imageInfo.sampler = tex.samplerIndex == -1
                              ? samplers.back().sampler
                              : samplers[tex.samplerIndex].sampler;
      imageInfo.imageView = tex.imageIndex == -1
                                ? images.back().image.imageView
                                : images[tex.imageIndex].image.imageView;
      // imageInfo.sampler = samplers[tex.samplerIndex].sampler;
      // imageInfo.imageView = images[tex.imageIndex].image.imageView;
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      writer.Write(mat.descriptorSet, binding,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
      binding++;
    }

    writer.Update(mDevice);
  };
  for (auto& mat : materials) {
    writeMaterial(mat);
  }
}

void glTFModel::Cleanup() {
  for (auto& node : nodes) {
    node.ubo.Unmap(mAllocator);
    node.ubo.Cleanup(mAllocator);
  }
  for (auto& image : images) {
    image.image.Cleanup(mDevice, mAllocator);
  }
  for (auto& sampler : samplers) {
    vkDestroySampler(mDevice, sampler.sampler, nullptr);
  }
  for (auto& mat : materials) {
    vkDestroyDescriptorSetLayout(mDevice, mat.materialSetLayout, nullptr);
  }
  vertices.Cleanup(mAllocator);
  indices.Cleanup(mAllocator);
  // vkDestroyDescriptorPool(mDevice, descritorPool, nullptr);
  // vkDestroyDescriptorSetLayout(mDevice, uboSetLayout, nullptr);
  // vkDestroyDescriptorSetLayout(mDevice, imageDescriptorSetLayout, nullptr);
}

}  // namespace hkr
