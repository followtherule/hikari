#include "Renderer/Pipeline.h"
#include "Util/Assert.h"
#include "Util/vk_debug.h"

namespace {

VkFormat RelativeOffsetToVkFormat(uint32_t offset0, uint32_t offset1) {
  HKR_ASSERT(offset1 - offset0 > 0);
  switch ((offset1 - offset0) / 4) {
    case 1:
      return VK_FORMAT_R32_SFLOAT;
    case 2:
      return VK_FORMAT_R32G32_SFLOAT;
    case 3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case 4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
      HKR_ASSERT(0);
  }
}

}  // namespace

namespace hkr {

void GraphicsPipelineBuilder::ShaderStage(
    std::initializer_list<ShaderInfo> shaderInfos) {
  shaderStageInfos.resize(shaderInfos.size());
  for (size_t i = 0; i < shaderStageInfos.size(); i++) {
    auto shaderInfo = shaderInfos.begin() + i;
    shaderStageInfos[i].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfos[i].module = shaderInfo->module;
    shaderStageInfos[i].stage = shaderInfo->stage;
    shaderStageInfos[i].pName = "main";
  }
}

void GraphicsPipelineBuilder::VertexInput(
    uint32_t stride,
    std::initializer_list<VertexAttributeInfo> attributeInfos) {
  bindingDescription.binding = 0;
  bindingDescription.stride = stride;
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  attributeDescriptions.resize(attributeInfos.size());
  for (size_t i = 0; i < attributeDescriptions.size(); i++) {
    auto attributeInfo = attributeInfos.begin() + i;
    attributeDescriptions[i].binding = 0;
    attributeDescriptions[i].location = i;
    attributeDescriptions[i].format = attributeInfo->format;
    attributeDescriptions[i].offset = attributeInfo->offset;
  }
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
}

void GraphicsPipelineBuilder::InputAssembly(VkPrimitiveTopology topology,
                                            VkBool32 primitiveRestartEnable) {
  inputAssemblyInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyInfo.topology = topology;
  inputAssemblyInfo.flags = 0;
  inputAssemblyInfo.primitiveRestartEnable = primitiveRestartEnable;
}

void GraphicsPipelineBuilder::Viewport() {
  viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportInfo.viewportCount = 1;
  viewportInfo.scissorCount = 1;
}

void GraphicsPipelineBuilder::Rasterization(VkCullModeFlags cullMode,
                                            VkFrontFace frontFace,
                                            VkPolygonMode polygonMode,
                                            float lineWidth) {
  rasterizationInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationInfo.depthClampEnable = VK_FALSE;
  rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
  rasterizationInfo.polygonMode = polygonMode;
  rasterizationInfo.cullMode = cullMode;
  rasterizationInfo.frontFace = frontFace;
  rasterizationInfo.lineWidth = lineWidth;
  rasterizationInfo.depthBiasEnable = VK_FALSE;
}

void GraphicsPipelineBuilder::Multisample(
    VkSampleCountFlagBits rasterizationSamples,
    VkBool32 sampleShadingEnable,
    float minSampleShading) {
  multisampleInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleInfo.rasterizationSamples = rasterizationSamples;
  multisampleInfo.sampleShadingEnable = sampleShadingEnable;
  multisampleInfo.minSampleShading = minSampleShading;
}

void GraphicsPipelineBuilder::DepthStencil(VkBool32 depthTestEnable,
                                           VkBool32 depthWriteEnable,
                                           VkCompareOp depthCompareOp) {
  depthStencilInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilInfo.depthTestEnable = depthTestEnable;
  depthStencilInfo.depthWriteEnable = depthWriteEnable;
  depthStencilInfo.depthCompareOp = depthCompareOp;
  depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
  depthStencilInfo.stencilTestEnable = VK_FALSE;
  depthStencilInfo.back.failOp = VK_STENCIL_OP_KEEP;
  depthStencilInfo.back.passOp = VK_STENCIL_OP_KEEP;
  depthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
}

void GraphicsPipelineBuilder::ColorBlend(VkBool32 blendEnable,
                                         VkBlendFactor srcColorBlendFactor,
                                         VkBlendFactor dstColorBlendFactor,
                                         VkBlendOp colorBlendOp) {
  colorBlendAttachment.blendEnable = blendEnable;
  colorBlendAttachment.srcColorBlendFactor = srcColorBlendFactor;
  colorBlendAttachment.dstColorBlendFactor = dstColorBlendFactor;
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  colorBlendInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendInfo.logicOpEnable = VK_FALSE;
  colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
  colorBlendInfo.attachmentCount = 1;
  colorBlendInfo.pAttachments = &colorBlendAttachment;
  colorBlendInfo.blendConstants[0] = 0.0f;
  colorBlendInfo.blendConstants[1] = 0.0f;
  colorBlendInfo.blendConstants[2] = 0.0f;
  colorBlendInfo.blendConstants[3] = 0.0f;
}

void GraphicsPipelineBuilder::DynamicState() {
  dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicInfo.pDynamicStates = dynamicStates.data();
}

void GraphicsPipelineBuilder::Rendering(uint32_t colorAttachmentCount,
                                        const VkFormat* pColorAttachmentFormats,
                                        VkFormat depthAttachmentFormat,
                                        VkFormat stencilAttachmentFormat) {
  renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  renderingInfo.colorAttachmentCount = colorAttachmentCount;
  renderingInfo.pColorAttachmentFormats = pColorAttachmentFormats;
  renderingInfo.depthAttachmentFormat = depthAttachmentFormat;
  renderingInfo.stencilAttachmentFormat = stencilAttachmentFormat;
}

VkPipeline GraphicsPipelineBuilder::Build(VkDevice device,
                                          VkPipelineLayout pipelineLayout) {
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = &renderingInfo;
  pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfos.size());
  pipelineInfo.pStages = shaderStageInfos.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
  pipelineInfo.pViewportState = &viewportInfo;
  pipelineInfo.pRasterizationState = &rasterizationInfo;
  pipelineInfo.pMultisampleState = &multisampleInfo;
  pipelineInfo.pDepthStencilState = &depthStencilInfo;
  pipelineInfo.pColorBlendState = &colorBlendInfo;
  pipelineInfo.pDynamicState = &dynamicInfo;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  VkPipeline graphicsPipeline = VK_NULL_HANDLE;
  VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                     nullptr, &graphicsPipeline));
  return graphicsPipeline;
}

}  // namespace hkr
