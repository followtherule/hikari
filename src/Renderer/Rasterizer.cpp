#include "Renderer/Rasterizer.h"
#include "Renderer/Buffer.h"
#include "Renderer/Descriptor.h"
#include "Renderer/Model.h"
#include "Renderer/Common.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Skybox.h"
#include "Util/Assert.h"
#include "Util/vk_util.h"
#include "Util/vk_debug.h"

#include <cstddef>
#include <cstdint>

namespace hkr {

void Rasterizer::Init(
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

  CreateAttachmentImage();

  CreateDescriptorPool();
  CreateDescriptorSetLayout();
  CreateDescriptorSets();

  CreatePipelineLayout();
  CreatePipeline();
}

void Rasterizer::CreateAttachmentImage() {
  // create off-screen color, depth images
  mColorImage.Create(
      mDevice, mAllocator, mWidth, mHeight, 1, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  mDepthImage.Create(mDevice, mAllocator, mWidth, mHeight, 1, FindDepthFormat(),
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void Rasterizer::CreateDescriptorPool() {
  const uint32_t imageCount = mModel->textures.size();
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount =
      static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * imageCount;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets =
      static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * (imageCount + 1);

  VK_CHECK(
      vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool));
}

void Rasterizer::CreateDescriptorSetLayout() {
  // one descriptor set for uniform buffer
  {
    DescriptorSetLayoutBuilder builder(1);
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                       VK_SHADER_STAGE_VERTEX_BIT);
    mUboDescriptorSetLayout = builder.Build(mDevice);
  }

  // one descriptor set for model's texture
  {
    DescriptorSetLayoutBuilder builder(1);
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    mImageDescriptorSetLayout = builder.Build(mDevice);
  }

  // DescriptorSetLayoutBuilder builder(2);
  // builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
  //                    VK_SHADER_STAGE_VERTEX_BIT);
  // builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
  //                    VK_SHADER_STAGE_FRAGMENT_BIT);
  // mDescriptorSetLayout = builder.Build(mDevice);
}

void Rasterizer::CreateDescriptorSets() {
  // ubo descriptor set
  std::vector<VkDescriptorSetLayout> uboSetLayouts(MAX_FRAMES_IN_FLIGHT,
                                                   mUboDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = mDescriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = uboSetLayouts.data();
  mUboDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  VK_CHECK(
      vkAllocateDescriptorSets(mDevice, &allocInfo, mUboDescriptorSets.data()));

  // image descriptor set
  const size_t imageCount = mModel->textures.size();
  std::vector<VkDescriptorSetLayout> imageSetLayouts(imageCount,
                                                     mImageDescriptorSetLayout);
  allocInfo.descriptorSetCount = static_cast<uint32_t>(imageCount);
  allocInfo.pSetLayouts = imageSetLayouts.data();
  mImageDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT,
                              std::vector<VkDescriptorSet>(imageCount));
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK(vkAllocateDescriptorSets(mDevice, &allocInfo,
                                      mImageDescriptorSets[i].data()));
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    DescriptorSetWriter writer(1);
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = mUniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);
    writer.Write(mUboDescriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 &bufferInfo);
    writer.Update(mDevice);

    for (size_t j = 0; j < imageCount; j++) {
      DescriptorSetWriter writer(1);
      const auto& texture = mModel->textures[j];
      VkDescriptorImageInfo imageInfo{};
      imageInfo.sampler = mModel->samplers[texture.samplerIndex].sampler;
      imageInfo.imageView = mModel->images[texture.imageIndex].image.imageView;
      // imageInfo.imageView = mModel->images.back().image.imageView;
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      writer.Write(mImageDescriptorSets[i][j], 0,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
      writer.Update(mDevice);
    }
  }
}

void Rasterizer::CreatePipelineCache() {
  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VK_CHECK(vkCreatePipelineCache(mDevice, &pipelineCacheCreateInfo, nullptr,
                                 &mPipelineCache));
}

void Rasterizer::CreatePipelineLayout() {
  VkPushConstantRange pushConstant{};
  pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConstant.offset = 0;
  pushConstant.size = sizeof(Mat4);
  std::array<VkDescriptorSetLayout, 2> setLayouts{mUboDescriptorSetLayout,
                                                  mImageDescriptorSetLayout};
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = setLayouts.size();
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
  VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr,
                                  &mPipelineLayout));
}

void Rasterizer::CreatePipeline() {
  GraphicsPipelineBuilder builder;

  // ShaderStage
  VkShaderModule vertShaderModule =
      LoadShaderModule(mDevice, mAssetPath + "spirv/shader.vert.spv");
  VkShaderModule fragShaderModule =
      LoadShaderModule(mDevice, mAssetPath + "spirv/shader.frag.spv");
  builder.ShaderStage({{VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule},
                       {VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule}});
  // VertexInput
  builder.VertexInput(
      sizeof(glTFVertex),
      {{VK_FORMAT_R32G32B32_SFLOAT, offsetof(glTFVertex, position)},
       {VK_FORMAT_R32G32B32_SFLOAT, offsetof(glTFVertex, normal)},
       {VK_FORMAT_R32G32_SFLOAT, offsetof(glTFVertex, uv)},
       {VK_FORMAT_R32G32B32_SFLOAT, offsetof(glTFVertex, color)}});

  //  InputAssembly
  builder.InputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  // Viewport
  builder.Viewport();

  // Rasterization
  builder.Rasterization(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
                        VK_POLYGON_MODE_FILL);

  // Multisample
  builder.Multisample();

  // DepthStencil
  builder.DepthStencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);

  // ColorBlend
  builder.ColorBlend();

  // DynamicState
  builder.DynamicState();

  // Dynamic Rendering
  VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
  builder.Rendering(1, &colorFormat, FindDepthFormat());

  // Build pipeline
  mGraphicsPipeline = builder.Build(mDevice, mPipelineLayout);

  vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
}

void Rasterizer::DrawNode(VkCommandBuffer commandBuffer,
                          uint32_t currentFrame,
                          const glTFNode& node) {
  if (node.meshIndex != -1 &&
      mModel->meshes[node.meshIndex].primitives.size() > 0) {
    vkCmdPushConstants(commandBuffer, mPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4),
                       &node.uniformData.globalTransform);
    for (const auto& primitive : mModel->meshes[node.meshIndex].primitives) {
      if (primitive.indexCount > 0) {
        const auto& material = mModel->materials[primitive.materialIndex];
        VkDescriptorSet imageDescriptorSet =
            mImageDescriptorSets[currentFrame][material.baseColorTextureIndex];
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mPipelineLayout, 1, 1, &imageDescriptorSet, 0,
                                nullptr);
        vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1,
                         primitive.firstIndex, 0, 0);
      }
    }
  }
  for (const auto& childIndex : node.childIndices) {
    DrawNode(commandBuffer, currentFrame, mModel->nodes[childIndex]);
  }
}

void Rasterizer::Draw(VkCommandBuffer commandBuffer, uint32_t currentFrame) {
  VkDeviceSize offsets[1] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mModel->vertices.buffer,
                         offsets);
  vkCmdBindIndexBuffer(commandBuffer, mModel->indices.buffer, 0,
                       VK_INDEX_TYPE_UINT32);
  for (uint32_t nodeIndex : mModel->topLevelNodeIndices) {
    const auto& node = mModel->nodes[nodeIndex];
    DrawNode(commandBuffer, currentFrame, node);
  }
}

void Rasterizer::RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                     uint32_t currentFrame,
                                     VkImage swapchainImage) {
  InsertImageMemoryBarrier(
      commandBuffer, mColorImage.image,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (mRequireStencil) {
    aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  InsertImageMemoryBarrier(commandBuffer, mDepthImage.image,
                           VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                               VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                           VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                               VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                           0, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                           VkImageSubresourceRange{aspectMask, 0, 1, 0, 1});

  // Color attachment
  VkRenderingAttachmentInfo colorAttachment{
      VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  colorAttachment.imageView = mColorImage.imageView;
  colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.clearValue.color = {0.2f, 0.3f, 0.3f, 0.0f};
  // Depth/stencil attachment
  VkRenderingAttachmentInfo depthStencilAttachment{
      VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  depthStencilAttachment.imageView = mDepthImage.imageView;
  depthStencilAttachment.imageLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthStencilAttachment.clearValue.depthStencil = {1.0f, 0};

  VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
  renderingInfo.renderArea.offset = {0, 0};
  renderingInfo.renderArea.extent = {static_cast<uint32_t>(mWidth),
                                     static_cast<uint32_t>(mHeight)};
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &colorAttachment;
  renderingInfo.pDepthAttachment = &depthStencilAttachment;
  if (mRequireStencil) {
    renderingInfo.pStencilAttachment = &depthStencilAttachment;
  } else {
    renderingInfo.pStencilAttachment = nullptr;
  }

  vkCmdBeginRendering(commandBuffer, &renderingInfo);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)mWidth;
  viewport.height = (float)mHeight;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {static_cast<uint32_t>(mWidth),
                    static_cast<uint32_t>(mHeight)};
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  mSkybox->Draw(commandBuffer, currentFrame);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    mGraphicsPipeline);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mPipelineLayout, 0, 1,
                          &mUboDescriptorSets[currentFrame], 0, nullptr);
  Draw(commandBuffer, currentFrame);

  vkCmdEndRendering(commandBuffer);
  // This barrier prepares the color image for presentation, we don't need to
  // care for the depth image
  InsertImageMemoryBarrier(
      commandBuffer, mColorImage.image,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  InsertImageMemoryBarrier(
      commandBuffer, swapchainImage,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  VkExtent2D extent{.width = static_cast<uint32_t>(mWidth),
                    .height = static_cast<uint32_t>(mHeight)};
  CopyImageToImage(commandBuffer, mColorImage.image, swapchainImage, extent,
                   extent);
}

VkFormat Rasterizer::FindDepthFormat() {
  if (mRequireStencil) {
    return FindSupportedFormat(
        {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT,
         VK_FORMAT_D16_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  } else {
    return FindSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
         VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT,
         VK_FORMAT_D16_UNORM},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  }
}

VkFormat Rasterizer::FindSupportedFormat(
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(mPhysDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  HKR_ASSERT(0);
}

void Rasterizer::OnResize(int width, int height) {
  mWidth = width;
  mHeight = height;
  mColorImage.Cleanup(mDevice, mAllocator);
  mDepthImage.Cleanup(mDevice, mAllocator);

  mColorImage.Create(
      mDevice, mAllocator, mWidth, mHeight, 1, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  mDepthImage.Create(mDevice, mAllocator, mWidth, mHeight, 1, FindDepthFormat(),
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void Rasterizer::Cleanup() {
  mColorImage.Cleanup(mDevice, mAllocator);
  mDepthImage.Cleanup(mDevice, mAllocator);

  vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
  vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
  vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);

  vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(mDevice, mUboDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(mDevice, mImageDescriptorSetLayout, nullptr);
}

}  // namespace hkr
