#include "Renderer/Raytracer.h"
#include "Renderer/Buffer.h"
#include "Renderer/Descriptor.h"
#include "Renderer/Common.h"
#include "Renderer/Model.h"
#include "Core/Math.h"
#include "Util/vk_debug.h"
#include "Util/vk_util.h"

#include <cstddef>
#include <cstdint>

namespace hkr {

// struct PushConstant {
//   VkDeviceAddress vertexBuffer;
//   VkDeviceAddress indexBuffer;
// };

void Raytracer::Init(
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
    const std::string& assetPath) {
  // setup rendering context
  mDevice = device;
  mPhysDevice = physDevice;
  mGraphicsQueue = queue;
  mCommandPool = commandPool;
  mModel = model;
  mSkybox = skybox;
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    mUniformBuffers[i] = uniformBuffers[i].buffer;
  }
  mAllocator = allocator;
  mSwapchainImageFormat = swapchainImageFormat;
  mWidth = width;
  mHeight = height;
  mAssetPath = assetPath;

  // get ray tracing pipeline properties and acceleration structure properties
  VkPhysicalDeviceRayTracingPipelinePropertiesKHR
      rayTracingPipelineProperties{};
  rayTracingPipelineProperties.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
  VkPhysicalDeviceProperties2 deviceProperties2{};
  deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  deviceProperties2.pNext = &rayTracingPipelineProperties;
  vkGetPhysicalDeviceProperties2(mPhysDevice, &deviceProperties2);
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
  asFeatures.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
  VkPhysicalDeviceFeatures2 deviceFeatures2{};
  deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  deviceFeatures2.pNext = &asFeatures;
  vkGetPhysicalDeviceFeatures2(mPhysDevice, &deviceFeatures2);

  mHandleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
  mHandleAlignment = rayTracingPipelineProperties.shaderGroupHandleAlignment;
  mBaseAlignment = rayTracingPipelineProperties.shaderGroupBaseAlignment;
  // clang-format off
  /*
    A shader binding table (SBT) consists of multiple "records", each record
    has a shader group handle referencing up to three shaders depending on
    its type:
    - ray generation record: only a ray generation shader
    - miss record: only a miss shader
    - hit record: a closest hit shader, an optional any hit shader, and optional intersection shader (only for procedural hit groups)
    - callable record: only a callable shader
    The rest of the record is a shader record, which can be any data, and accessible via shaderRecordEXT.
 
    SBT = mulitple records
    record = shader group handle + shader record
    shader group handle = references up to three shaders

    The stride between records in SBT must:
    - >= rayTracingPipelineProperties.shaderGroupHandleSize
    - <= rayTracingPipelineProperties.maxShaderGroupStride
    - be a multiple of rayTracingPipelineProperties.shaderGroupHandleAlignment
    - each SBT starts at a multiple of rayTracingPipelineProperties.shaderGroupBaseAlignment
  */
  // clang-format on

  BuildBLAS();
  BuildTLAS();

  CreateStorageImage();
  CreateDescriptorPool();
  CreateDescriptorSetLayout();
  CreateDescriptorSets();
  CreatePipelineLayout();
  CreatePipeline();
  CreateShaderBindingTables();
}

void Raytracer::CreateStorageImage() {
  mStorageImage.Create(
      mDevice, mAllocator, mWidth, mHeight, 1, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
  VkCommandBuffer commandBuffer = BeginOneTimeCommands(mDevice, mCommandPool);
  InsertImageMemoryBarrier(
      commandBuffer, mStorageImage.image, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  EndOneTimeCommands(mDevice, mGraphicsQueue, mCommandPool, commandBuffer);
}

void Raytracer::BuildBLAS() {
  // Create transform matrices, one VkTransformMatrixKHR for each gltf
  // primitive. Note that gltf primitive is different from the primitive in
  // VkAccelerationStructureBuildRangeInfoKHR (which is a triangle in this
  // case).
  uint32_t vertexCount = 0;
  std::vector<VkTransformMatrixKHR> transformMatrices;
  for (uint32_t nodeIndex : mModel->nodeIndices) {
    const auto& node = mModel->nodes[nodeIndex];
    if (node.meshIndex != -1) {
      const auto& primitives = mModel->meshes[node.meshIndex].primitives;
      for (const auto& primitive : primitives) {
        if (primitive.indexCount > 0) {
          vertexCount += primitive.vertexCount;
          auto& transform = transformMatrices.emplace_back();
          auto matrix =
              glm::mat3x4(glm::transpose(node.uniformData.globalTransform));
          memcpy(&transform, &matrix, sizeof(glm::mat3x4));
        }
      }
    }
  }

  // upload transform matrices data to gpu
  VkBufferUsageFlags2 usage =
      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
      VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
      VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  Buffer transformBuffer;
  VkDeviceSize transformSize =
      sizeof(VkTransformMatrixKHR) * transformMatrices.size();
  transformBuffer.Create(mAllocator, transformSize, usage);
  {
    StagingBuffer staging;
    staging.Create(mAllocator, transformSize);
    staging.Map(mAllocator);
    staging.Write(transformMatrices.data(), transformSize);
    staging.Unmap(mAllocator);
    VkCommandBuffer commandBuffer = BeginOneTimeCommands(mDevice, mCommandPool);
    CopyBufferToBuffer(commandBuffer, staging.buffer, transformBuffer.buffer,
                       transformSize);
    EndOneTimeCommands(mDevice, mGraphicsQueue, mCommandPool, commandBuffer);
    staging.Cleanup(mAllocator);
  }

  // Create geometries, one VkAccelerationStructureGeometryKHR,
  // VkAccelerationStructureBuildRangeInfoKHR, GeometryNode for each gltf
  // primitive.
  std::vector<VkAccelerationStructureGeometryKHR> geometries;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
  std::vector<GeometryNode> geometryNodes;  // storage buffer descritor
  std::vector<uint32_t> maxPrimitiveCounts;
  for (int nodeIndex : mModel->nodeIndices) {
    const auto& node = mModel->nodes[nodeIndex];
    if (node.meshIndex != -1) {
      const auto& primitives = mModel->meshes[node.meshIndex].primitives;
      for (const auto& primitive : primitives) {
        if (primitive.indexCount > 0) {
          VkDeviceOrHostAddressConstKHR vertexBufferAddr;
          vertexBufferAddr.deviceAddress =
              GetBufferDeviceAddress(mDevice, mModel->vertices.buffer);
          VkDeviceOrHostAddressConstKHR indexBufferAddr;
          indexBufferAddr.deviceAddress =
              GetBufferDeviceAddress(mDevice, mModel->indices.buffer) +
              primitive.firstIndex * sizeof(uint32_t);
          VkDeviceOrHostAddressConstKHR transformBufferAddr;
          transformBufferAddr.deviceAddress =
              GetBufferDeviceAddress(mDevice, transformBuffer.buffer) +
              geometries.size() * sizeof(VkTransformMatrixKHR);
          VkAccelerationStructureGeometryKHR geometry{};
          geometry.sType =
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
          geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
          geometry.geometry.triangles.sType =
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
          geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
          geometry.geometry.triangles.vertexData = vertexBufferAddr;
          geometry.geometry.triangles.maxVertex = vertexCount - 1;
          // geometry.geometry.triangles.maxVertex = primitive->vertexCount;
          geometry.geometry.triangles.vertexStride = sizeof(glTFVertex);
          geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
          geometry.geometry.triangles.indexData = indexBufferAddr;
          geometry.geometry.triangles.transformData = transformBufferAddr;
          geometries.push_back(geometry);
          maxPrimitiveCounts.push_back(primitive.indexCount / 3);

          VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
          buildRangeInfo.primitiveCount = primitive.indexCount / 3;
          buildRangeInfo.primitiveOffset = 0;
          buildRangeInfo.firstVertex = 0;
          buildRangeInfo.transformOffset = 0;
          buildRangeInfos.push_back(buildRangeInfo);

          GeometryNode geometryNode{};
          geometryNode.vertexBufferDeviceAddr = vertexBufferAddr.deviceAddress;
          geometryNode.indexBufferDeviceAddr = indexBufferAddr.deviceAddress;
          if (primitive.materialIndex != -1) {
            const auto& material = mModel->materials[primitive.materialIndex];
            geometryNode.BaseColorTextureIndex = material.baseColorTextureIndex;
            geometryNode.OcclusionTextureIndex = material.occlusionTextureIndex;
            geometryNode.NormalTextureIndex = material.normalTextureIndex;
          }
          geometryNodes.push_back(geometryNode);
        }
      }
    }
  }
  std::vector<VkAccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos(
      buildRangeInfos.size());
  for (size_t i = 0; i < pBuildRangeInfos.size(); i++) {
    pBuildRangeInfos[i] = &buildRangeInfos[i];
  }
  // Upload geometry node data to gpu buffer.
  {
    uint32_t geometryNodeSize = geometryNodes.size() * sizeof(GeometryNode);
    mGeometryNodeBuffer.Create(mAllocator, geometryNodeSize,
                               VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
                                   VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT);
    StagingBuffer staging;
    staging.Create(mAllocator, geometryNodeSize);
    staging.Map(mAllocator);
    staging.Write(geometryNodes.data(), geometryNodeSize);
    staging.Unmap(mAllocator);
    VkCommandBuffer commandBuffer = BeginOneTimeCommands(mDevice, mCommandPool);
    CopyBufferToBuffer(commandBuffer, staging.buffer,
                       mGeometryNodeBuffer.buffer, geometryNodeSize);
    EndOneTimeCommands(mDevice, mGraphicsQueue, mCommandPool, commandBuffer);
    staging.Cleanup(mAllocator);
  }
  // Get acceleration structure buffer/scratch buffer size infos.
  VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
  VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
  {
    buildGeometryInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildGeometryInfo.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildGeometryInfo.geometryCount = static_cast<uint32_t>(geometries.size());
    buildGeometryInfo.pGeometries = geometries.data();

    buildSizesInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        mDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildGeometryInfo, maxPrimitiveCounts.data(), &buildSizesInfo);
  }
  // Create BLAS buffer and BLAS.
  mBLAS.buffer.Create(mAllocator, buildSizesInfo.accelerationStructureSize,
                      VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
  VkAccelerationStructureCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  createInfo.buffer = mBLAS.buffer.buffer;
  createInfo.size = buildSizesInfo.accelerationStructureSize;
  createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  vkCreateAccelerationStructureKHR(mDevice, &createInfo, nullptr, &mBLAS.AS);

  // create scratch buffer
  Buffer scratchBuffer;
  scratchBuffer.Create(mAllocator, buildSizesInfo.buildScratchSize,
                       VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                           VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

  // Build BLAS and get BLAS's device address.
  buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildGeometryInfo.dstAccelerationStructure = mBLAS.AS;
  buildGeometryInfo.scratchData.deviceAddress =
      GetBufferDeviceAddress(mDevice, scratchBuffer.buffer);
  VkCommandBuffer cmdBuf = BeginOneTimeCommands(mDevice, mCommandPool);
  vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildGeometryInfo,
                                      pBuildRangeInfos.data());
  EndOneTimeCommands(mDevice, mGraphicsQueue, mCommandPool, cmdBuf);

  VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddress{};
  asDeviceAddress.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  asDeviceAddress.accelerationStructure = mBLAS.AS;
  mBLAS.deviceAddress =
      vkGetAccelerationStructureDeviceAddressKHR(mDevice, &asDeviceAddress);
  scratchBuffer.Cleanup(mAllocator);
  transformBuffer.Cleanup(mAllocator);
}

void Raytracer::BuildTLAS() {
  VkTransformMatrixKHR transform{
      // clang-format off
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
      // clang-format on
  };

  VkAccelerationStructureInstanceKHR instance{};
  instance.transform = transform;
  instance.instanceCustomIndex = 0;
  instance.mask = 0xFF;
  instance.instanceShaderBindingTableRecordOffset = 0;
  instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  instance.accelerationStructureReference = mBLAS.deviceAddress;
  // create instance buffer
  Buffer instanceBuffer;
  VkDeviceSize instanceSize = sizeof(VkAccelerationStructureInstanceKHR);
  instanceBuffer.Create(
      mAllocator, instanceSize,
      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  StagingBuffer staging;
  staging.Create(mAllocator, instanceSize);
  staging.Map(mAllocator);
  staging.Write(&instance, instanceSize);
  staging.Unmap(mAllocator);
  VkCommandBuffer commandBuffer = BeginOneTimeCommands(mDevice, mCommandPool);
  CopyBufferToBuffer(commandBuffer, staging.buffer, instanceBuffer.buffer,
                     instanceSize);
  EndOneTimeCommands(mDevice, mGraphicsQueue, mCommandPool, commandBuffer);
  staging.Cleanup(mAllocator);

  VkDeviceOrHostAddressConstKHR instanceBufferAddr{};
  instanceBufferAddr.deviceAddress =
      GetBufferDeviceAddress(mDevice, instanceBuffer.buffer);

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.geometry.instances.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  geometry.geometry.instances.arrayOfPointers = VK_FALSE;
  geometry.geometry.instances.data = instanceBufferAddr;

  // get acceleration structure buffer/scratch buffer size infos
  VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
  VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
  {
    buildGeometryInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildGeometryInfo.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildGeometryInfo.geometryCount = 1;
    buildGeometryInfo.pGeometries = &geometry;
    uint32_t primitive_count = 1;

    buildSizesInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        mDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildGeometryInfo, &primitive_count, &buildSizesInfo);
  }
  // create TLAS buffer and TLAS
  mTLAS.buffer.Create(mAllocator, buildSizesInfo.accelerationStructureSize,
                      VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
  VkAccelerationStructureCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  createInfo.buffer = mTLAS.buffer.buffer;
  createInfo.size = buildSizesInfo.accelerationStructureSize;
  createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  vkCreateAccelerationStructureKHR(mDevice, &createInfo, nullptr, &mTLAS.AS);

  // create scratch buffer
  Buffer scratchBuffer;
  scratchBuffer.Create(mAllocator, buildSizesInfo.buildScratchSize,
                       VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                           VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

  // build TLAS
  buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildGeometryInfo.dstAccelerationStructure = mTLAS.AS;
  buildGeometryInfo.scratchData.deviceAddress =
      GetBufferDeviceAddress(mDevice, scratchBuffer.buffer);
  VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
  buildRangeInfo.primitiveCount = 1;
  buildRangeInfo.primitiveOffset = 0;
  buildRangeInfo.firstVertex = 0;
  buildRangeInfo.transformOffset = 0;
  VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;
  VkCommandBuffer cmdBuf = BeginOneTimeCommands(mDevice, mCommandPool);
  vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildGeometryInfo,
                                      &pBuildRangeInfo);
  EndOneTimeCommands(mDevice, mGraphicsQueue, mCommandPool, cmdBuf);

  VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddress{};
  asDeviceAddress.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  asDeviceAddress.accelerationStructure = mTLAS.AS;
  mTLAS.deviceAddress =
      vkGetAccelerationStructureDeviceAddressKHR(mDevice, &asDeviceAddress);
  scratchBuffer.Cleanup(mAllocator);
  instanceBuffer.Cleanup(mAllocator);
}

void Raytracer::CreateDescriptorPool() {
  std::array<VkDescriptorPoolSize, 6> poolSizes{
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                           MAX_FRAMES_IN_FLIGHT},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                           MAX_FRAMES_IN_FLIGHT},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                           MAX_FRAMES_IN_FLIGHT},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           MAX_FRAMES_IN_FLIGHT},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                           MAX_FRAMES_IN_FLIGHT},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           static_cast<uint32_t>(mModel->textures.size()) *
                               MAX_FRAMES_IN_FLIGHT},
  };

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  // poolInfo.maxSets = 1;
  poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VK_CHECK(
      vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool));
}

void Raytracer::CreateDescriptorSetLayout() {
  DescriptorSetLayoutBuilder layoutBuilder(6);

  // TLAS
  layoutBuilder.AddBinding(
      0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
      VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // storage image for off-screen rendering
  layoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // uniform buffer for camera data and light position
  layoutBuilder.AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                               VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                               VK_SHADER_STAGE_MISS_BIT_KHR);
  // cubemap
  layoutBuilder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_MISS_BIT_KHR);
  // geometry node storage buffer for access to vertex/index buffer and index
  // into textures descriptors
  layoutBuilder.AddBinding(
      4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
  // model's textures
  layoutBuilder.AddBinding(
      5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
      mModel->textures.size());
  mDescriptorSetLayout = layoutBuilder.Build(mDevice, true);
}

void Raytracer::CreateDescriptorSets() {
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                             mDescriptorSetLayout);

  VkDescriptorSetVariableDescriptorCountAllocateInfo
      variableDescriptorCountAllocInfo{};
  const uint32_t imageCount = static_cast<uint32_t>(mModel->textures.size());
  std::vector<uint32_t> imageCounts(MAX_FRAMES_IN_FLIGHT, imageCount);
  variableDescriptorCountAllocInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
  variableDescriptorCountAllocInfo.descriptorSetCount =
      static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  variableDescriptorCountAllocInfo.pDescriptorCounts = imageCounts.data();

  VkDescriptorSetAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocateInfo.pNext = &variableDescriptorCountAllocInfo;
  allocateInfo.descriptorPool = mDescriptorPool;
  allocateInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocateInfo.pSetLayouts = layouts.data();
  mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  VK_CHECK(
      vkAllocateDescriptorSets(mDevice, &allocateInfo, mDescriptorSets.data()));

  // image infos in gltf model
  std::vector<VkDescriptorImageInfo> imageInfos(imageCount);
  for (size_t i = 0; i < imageCount; i++) {
    imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const auto& texture = mModel->textures[i];
    imageInfos[i].imageView =
        mModel->images[texture.imageIndex].image.imageView;
    imageInfos[i].sampler = mModel->samplers[texture.samplerIndex].sampler;
  }
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    DescriptorSetWriter writer(6);

    // TLAS
    VkWriteDescriptorSetAccelerationStructureKHR writeAS{};
    writeAS.sType =
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    writeAS.accelerationStructureCount = 1;
    writeAS.pAccelerationStructures = &mTLAS.AS;
    writer.Write(mDescriptorSets[i], 0,
                 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, &writeAS);
    // storage image
    VkDescriptorImageInfo storageImage{};
    storageImage.imageView = mStorageImage.imageView;
    storageImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    writer.Write(mDescriptorSets[i], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                 &storageImage);

    // uniform buffer
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = mUniformBuffers[i];
    uboInfo.range = sizeof(UniformBufferObject);
    writer.Write(mDescriptorSets[i], 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 &uboInfo);

    // cubemap
    VkDescriptorImageInfo cubemap{};
    cubemap.imageView = mSkybox->cubemap.imageView;
    cubemap.sampler = mSkybox->cubemapSampler;
    cubemap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    writer.Write(mDescriptorSets[i], 3,
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &cubemap);

    // storage buffer
    VkDescriptorBufferInfo ssboInfo{};
    ssboInfo.buffer = mGeometryNodeBuffer.buffer;
    ssboInfo.offset = 0;
    ssboInfo.range = VK_WHOLE_SIZE;
    writer.Write(mDescriptorSets[i], 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                 &ssboInfo);
    // textures in gltf model
    writer.Write(mDescriptorSets[i], 5,
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount,
                 imageInfos.data());

    writer.Update(mDevice);
  }
}

void Raytracer::CreatePipelineLayout() {
  // VkPushConstantRange pushConstant{};
  // pushConstant.offset = 0;
  // pushConstant.size = sizeof(PushConstant);
  // pushConstant.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;
  // pipelineLayoutInfo.pushConstantRangeCount = 1;
  // pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
  VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr,
                                  &mPipelineLayout));
}

void Raytracer::CreatePipeline() {
  // raygen SBT with one record: raygen
  // miss SBT with two records: miss, shadow
  // hit SBT with one record: closesthit + anyhit
  std::array<VkPipelineShaderStageCreateInfo, 5> shaderStages;
  std::array<VkRayTracingShaderGroupCreateInfoKHR, 4> shaderGroups;

  std::array<VkShaderModule, 5> shaderModules{
      LoadShaderModule(mDevice, mAssetPath + "spirv/raygen.rgen.spv"),
      LoadShaderModule(mDevice, mAssetPath + "spirv/miss.rmiss.spv"),
      LoadShaderModule(mDevice, mAssetPath + "spirv/shadow.rmiss.spv"),
      LoadShaderModule(mDevice, mAssetPath + "spirv/closesthit.rchit.spv"),
      LoadShaderModule(mDevice, mAssetPath + "spirv/anyhit.rahit.spv"),
  };
  // one ray generation group (raygen)
  {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.pName = "main";
    shaderStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    shaderStageInfo.module = shaderModules[0];
    shaderStages[0] = shaderStageInfo;

    VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
    shaderGroup.sType =
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroup.generalShader = 0;
    shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[0] = shaderGroup;
  }

  // two miss groups (miss, shadow)
  {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.pName = "main";
    shaderStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    shaderStageInfo.module = shaderModules[1];
    shaderStages[1] = shaderStageInfo;

    VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
    shaderGroup.sType =
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroup.generalShader = 1;
    shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[1] = shaderGroup;

    shaderStageInfo.module = shaderModules[2];
    shaderStages[2] = shaderStageInfo;
    shaderGroup.generalShader = 2;
    shaderGroups[2] = shaderGroup;
  }

  // one hit group (closesthit + anyhit)
  {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    shaderStageInfo.module = shaderModules[3];
    shaderStageInfo.pName = "main";
    shaderStages[3] = shaderStageInfo;

    shaderStageInfo.module = shaderModules[4];
    shaderStageInfo.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    shaderStages[4] = shaderStageInfo;

    VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
    shaderGroup.sType =
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.closestHitShader = 3;
    shaderGroup.anyHitShader = 4;
    shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[3] = shaderGroup;
  }

  VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
  pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.groupCount = shaderGroups.size();
  pipelineInfo.pGroups = shaderGroups.data();
  pipelineInfo.maxPipelineRayRecursionDepth = 1;
  pipelineInfo.layout = mPipelineLayout;
  VK_CHECK(vkCreateRayTracingPipelinesKHR(mDevice, VK_NULL_HANDLE,
                                          VK_NULL_HANDLE, 1, &pipelineInfo,
                                          nullptr, &mRaytracingPipeline));
  for (auto shaderModule : shaderModules) {
    vkDestroyShaderModule(mDevice, shaderModule, nullptr);
  }
}

void Raytracer::CreateShaderBindingTables() {
  // raygen SBT with one record: raygen
  // miss SBT with two records: miss, shadow
  // hit SBT with one record: closesthit + anyhit
  const uint32_t handleSizeAligned = AlignedSize(mHandleSize, mHandleAlignment);
  const uint32_t groupCount = 4;
  const uint32_t sbtSize = groupCount * mHandleSize;

  std::vector<uint8_t> shaderHandleStorage(sbtSize);
  VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(mDevice, mRaytracingPipeline, 0,
                                                groupCount, sbtSize,
                                                shaderHandleStorage.data()));
  const VkBufferUsageFlags usage =
      VK_BUFFER_USAGE_2_SHADER_BINDING_TABLE_BIT_KHR |
      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;

  // raygen SBT
  {
    mRaygenShaderBindingTable.Create(
        mAllocator,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT,
        mHandleSize, usage);
    mRaygenSBTAddr.deviceAddress =
        GetBufferDeviceAddress(mDevice, mRaygenShaderBindingTable.buffer);
    mRaygenSBTAddr.stride = handleSizeAligned;
    mRaygenSBTAddr.size = handleSizeAligned;
    mRaygenShaderBindingTable.Map(mAllocator);
    mRaygenShaderBindingTable.Write(shaderHandleStorage.data(), mHandleSize);
  }
  // miss SBT
  {
    mMissShaderBindingTable.Create(
        mAllocator,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT,
        mHandleSize * 2, usage);
    mMissSBTAddr.deviceAddress =
        GetBufferDeviceAddress(mDevice, mMissShaderBindingTable.buffer);
    mMissSBTAddr.stride = handleSizeAligned;
    mMissSBTAddr.size = handleSizeAligned * 2;
    mMissShaderBindingTable.Map(mAllocator);
    mMissShaderBindingTable.Write(shaderHandleStorage.data() + mHandleSize,
                                  mHandleSize * 2);
  }
  // hit SBT
  {
    mHitShaderBindingTable.Create(
        mAllocator,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT,
        mHandleSize, usage);
    mHitSBTAddr.deviceAddress =
        GetBufferDeviceAddress(mDevice, mHitShaderBindingTable.buffer);
    mHitSBTAddr.stride = handleSizeAligned;
    mHitSBTAddr.size = handleSizeAligned;
    mHitShaderBindingTable.Map(mAllocator);
    mHitShaderBindingTable.Write(shaderHandleStorage.data() + mHandleSize * 3,
                                 mHandleSize);
  }
}

void Raytracer::RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                    uint32_t currentFrame,
                                    VkImage swapchainImage) {
  VkStridedDeviceAddressRegionKHR callableShaderSBTAddr{};

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                    mRaytracingPipeline);
  // PushConstant pushConstant;
  // pushConstant.vertexBuffer = mVertexBufferDeviceAddress;
  // pushConstant.indexBuffer = mIndexBufferDeviceAddress;
  // vkCmdPushConstants(commandBuffer, mPipelineLayout,
  //                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0,
  //                    sizeof(PushConstant), &pushConstant);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                          mPipelineLayout, 0, 1, &mDescriptorSets[currentFrame],
                          0, 0);

  vkCmdTraceRaysKHR(commandBuffer, &mRaygenSBTAddr, &mMissSBTAddr, &mHitSBTAddr,
                    &callableShaderSBTAddr, mWidth, mHeight, 1);

  // copy storage image to swapchain image
  VkImageSubresourceRange subresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,
                                           1};
  InsertImageMemoryBarrier(
      commandBuffer, mStorageImage.image, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0, VK_ACCESS_2_TRANSFER_READ_BIT,
      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      subresourceRange);
  InsertImageMemoryBarrier(
      commandBuffer, swapchainImage, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      subresourceRange);

  VkExtent2D extent{.width = static_cast<uint32_t>(mWidth),
                    .height = static_cast<uint32_t>(mHeight)};
  CopyImageToImage(commandBuffer, mStorageImage.image, swapchainImage, extent,
                   extent);

  InsertImageMemoryBarrier(
      commandBuffer, mStorageImage.image, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_TRANSFER_READ_BIT, 0,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
      subresourceRange);
}

void Raytracer::OnResize(int width, int height) {
  mWidth = width;
  mHeight = height;
  mStorageImage.Cleanup(mDevice, mAllocator);
  CreateStorageImage();

  VkDescriptorImageInfo storageImage{};
  storageImage.imageView = mStorageImage.imageView;
  storageImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    DescriptorSetWriter writer(1);
    writer.Write(mDescriptorSets[i], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                 &storageImage);
    writer.Update(mDevice);
  }
}

void Raytracer::Cleanup() {
  mStorageImage.Cleanup(mDevice, mAllocator);
  vkDestroyPipeline(mDevice, mRaytracingPipeline, nullptr);
  vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
  vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);

  vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);

  mBLAS.buffer.Cleanup(mAllocator);
  vkDestroyAccelerationStructureKHR(mDevice, mBLAS.AS, nullptr);
  mTLAS.buffer.Cleanup(mAllocator);
  vkDestroyAccelerationStructureKHR(mDevice, mTLAS.AS, nullptr);
  mGeometryNodeBuffer.Cleanup(mAllocator);

  mRaygenShaderBindingTable.Unmap(mAllocator);
  mRaygenShaderBindingTable.Cleanup(mAllocator);
  mMissShaderBindingTable.Unmap(mAllocator);
  mMissShaderBindingTable.Cleanup(mAllocator);
  mHitShaderBindingTable.Unmap(mAllocator);
  mHitShaderBindingTable.Cleanup(mAllocator);
}

}  // namespace hkr
