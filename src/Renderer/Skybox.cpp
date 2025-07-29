#include "Renderer/Skybox.h"
#include "Renderer/Cube.h"
#include "Renderer/Image.h"
#include "Renderer/Common.h"
#include "Renderer/Descriptor.h"
#include "Renderer/Pipeline.h"
#include "Util/vk_util.h"
#include "Util/vk_debug.h"

#include <cstdint>

namespace hkr {

void Skybox::Create(
    VkDevice device,
    VkQueue queue,
    VkCommandPool commandPool,
    const std::array<UniformBuffer, MAX_FRAMES_IN_FLIGHT>& uniformBuffers,
    VmaAllocator allocator,
    const std::string& assetPath,
    const std::string& cubemapRelPath,
    VkBufferUsageFlags2 bufferUsageFlags) {
  // setup rendering context
  mDevice = device;
  mAssetPath = assetPath;
  mCube.Create(device, queue, commandPool, allocator, bufferUsageFlags);
  cubemap.Load(mDevice, allocator, queue, commandPool,
               mAssetPath + cubemapRelPath);
  SamplerBuilder builder;
  builder.SetMaxAnisotropy(8.0f);
  cubemapSampler = builder.Build(mDevice);
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    mUniformBuffers[i] = uniformBuffers[i].buffer;
  }

  CreateDescriptorPool();
  CreateDescriptorSetLayout();
  CreateDescriptorSets();
  CreatePipelineLayout();
  CreatePipeline();
}

void Skybox::CreateDescriptorPool() {
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

  VK_CHECK(
      vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool));
}

void Skybox::CreateDescriptorSetLayout() {
  // one descriptor set for uniform buffer (camera's data) and cubemap
  DescriptorSetLayoutBuilder builder(2);
  builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                     VK_SHADER_STAGE_VERTEX_BIT);
  builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     VK_SHADER_STAGE_FRAGMENT_BIT);
  mDescriptorSetLayout = builder.Build(mDevice);
}

void Skybox::CreateDescriptorSets() {
  std::vector<VkDescriptorSetLayout> setLayouts(MAX_FRAMES_IN_FLIGHT,
                                                mDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = mDescriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = setLayouts.data();
  mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  VK_CHECK(
      vkAllocateDescriptorSets(mDevice, &allocInfo, mDescriptorSets.data()));

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    DescriptorSetWriter writer(2);
    // unifrom buffer
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = mUniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);
    writer.Write(mDescriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 &bufferInfo);

    // cubemap
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = cubemapSampler;
    imageInfo.imageView = cubemap.imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writer.Write(mDescriptorSets[i], 1,
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);

    writer.Update(mDevice);
  }
}

void Skybox::CreatePipelineLayout() {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;
  VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr,
                                  &mPipelineLayout));
}

void Skybox::CreatePipeline() {
  GraphicsPipelineBuilder builder;

  // ShaderStage
  VkShaderModule vertShaderModule =
      LoadShaderModule(mDevice, mAssetPath + "spirv/skybox.vert.spv");
  VkShaderModule fragShaderModule =
      LoadShaderModule(mDevice, mAssetPath + "spirv/skybox.frag.spv");
  builder.ShaderStage({{VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule},
                       {VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule}});
  // VertexInput
  builder.VertexInput(sizeof(CubeVertex), {{VK_FORMAT_R32G32B32_SFLOAT,
                                            offsetof(CubeVertex, pos)}});

  //  InputAssembly
  builder.InputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  // Viewport
  builder.Viewport();

  // Rasterization
  builder.Rasterization(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
                        VK_POLYGON_MODE_FILL);

  // Multisample
  builder.Multisample();

  // DepthStencil
  builder.DepthStencil(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS);

  // ColorBlend
  builder.ColorBlend();

  // DynamicState
  builder.DynamicState();

  // Dynamic Rendering
  VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
  builder.Rendering(1, &colorFormat, VK_FORMAT_D32_SFLOAT);

  // Build pipeline
  mPipeline = builder.Build(mDevice, mPipelineLayout);

  vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
}

void Skybox::Draw(VkCommandBuffer commandBuffer, uint32_t currentFrame) {
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mPipelineLayout, 0, 1, &mDescriptorSets[currentFrame],
                          0, nullptr);
  VkDeviceSize offsets[1] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mCube.vertices.buffer, offsets);
  vkCmdBindIndexBuffer(commandBuffer, mCube.indices.buffer, 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(commandBuffer, 6 * 6, 1, 0, 0, 0);
}

void Skybox::Cleanup(VmaAllocator allocator) {
  mCube.Cleanup(allocator);
  cubemap.Cleanup(mDevice, allocator);
  vkDestroySampler(mDevice, cubemapSampler, nullptr);

  vkDestroyPipeline(mDevice, mPipeline, nullptr);
  vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);

  vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
}

}  // namespace hkr
