#pragma once

#include <volk.h>

#include <initializer_list>
#include <vector>
#include <array>

namespace hkr {

struct ShaderInfo {
  VkShaderStageFlagBits stage;
  VkShaderModule module;
};

struct VertexAttributeInfo {
  VkFormat format;
  uint32_t offset;
};

class GraphicsPipelineBuilder {
public:
  void ShaderStage(std::initializer_list<ShaderInfo> shaderInfos);
  void VertexInput(uint32_t stride,
                   std::initializer_list<VertexAttributeInfo> attributeInfos);
  void InputAssembly(VkPrimitiveTopology topology,
                     VkBool32 primitiveRestartEnable = VK_FALSE);
  void Viewport();
  void Rasterization(VkCullModeFlags cullMode,
                     VkFrontFace frontFace,
                     VkPolygonMode polygonMode,
                     float lineWidth = 1.0f);
  void Multisample(
      VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      VkBool32 sampleShadingEnable = VK_FALSE,
      float minSampleShading = 0.2f);
  void DepthStencil(VkBool32 depthTestEnable,
                    VkBool32 depthWriteEnable,
                    VkCompareOp depthCompareOp);
  void ColorBlend(
      VkBool32 blendEnable = VK_FALSE,
      VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      VkBlendOp colorBlendOp = VK_BLEND_OP_ADD);
  void DynamicState();
  void Rendering(uint32_t colorAttachmentCount,
                 const VkFormat* pColorAttachmentFormats,
                 VkFormat depthAttachmentFormat,
                 VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED);
  VkPipeline Build(VkDevice device, VkPipelineLayout pipelineLayout);

private:
  // ShaderStage
  std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos;

  // VertexInput
  VkVertexInputBindingDescription bindingDescription{};
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};

  // InputAssembly
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};

  // Viewport
  VkPipelineViewportStateCreateInfo viewportInfo{};

  // Rasterization
  VkPipelineRasterizationStateCreateInfo rasterizationInfo{};

  // MultiSample
  VkPipelineMultisampleStateCreateInfo multisampleInfo{};

  // DepthStencil
  VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};

  // ColorBlend
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  VkPipelineColorBlendStateCreateInfo colorBlendInfo{};

  // Dynamic
  std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicInfo{};

  // Rendering
  VkPipelineRenderingCreateInfo renderingInfo{};
};

}  // namespace hkr
