#include "Renderer/Descriptor.h"
#include "Util/vk_debug.h"

namespace hkr {

void DescriptorSetLayoutBuilder::AddBinding(uint32_t binding,
                                            VkDescriptorType descriptorType,
                                            VkShaderStageFlags stageFlags,
                                            uint32_t descriptorCount) {
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = descriptorType;
  layoutBinding.descriptorCount = descriptorCount;
  layoutBinding.pImmutableSamplers = nullptr;
  layoutBinding.stageFlags = stageFlags;
  mBindings[binding] = layoutBinding;
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::Build(
    VkDevice device,
    bool variableDescriptor) {
  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(mBindings.size());
  layoutInfo.pBindings = mBindings.data();

  std::vector<VkDescriptorBindingFlags> descriptorBindingFlags;
  if (variableDescriptor) {
    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
    setLayoutBindingFlags.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    setLayoutBindingFlags.bindingCount =
        static_cast<uint32_t>(mBindings.size());
    descriptorBindingFlags.resize(mBindings.size(), 0);
    descriptorBindingFlags.back() =
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();
    layoutInfo.pNext = &setLayoutBindingFlags;
  }
  VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout));
  return layout;
}

void DescriptorSetWriter::Write(VkDescriptorSet descriptorSet,
                                uint32_t binding,
                                VkDescriptorType descriptorType,
                                uint32_t descriptorCount,
                                VkDescriptorBufferInfo* pBufferInfo) {
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptorSet;
  write.dstBinding = binding;
  write.descriptorType = descriptorType;
  write.descriptorCount = descriptorCount;
  write.pBufferInfo = pBufferInfo;
  mWrites[index] = write;
  index++;
}

void DescriptorSetWriter::Write(VkDescriptorSet descriptorSet,
                                uint32_t binding,
                                VkDescriptorType descriptorType,
                                uint32_t descriptorCount,
                                VkDescriptorImageInfo* pImageInfo) {
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptorSet;
  write.dstBinding = binding;
  write.descriptorType = descriptorType;
  write.descriptorCount = descriptorCount;
  write.pImageInfo = pImageInfo;
  mWrites[index] = write;
  index++;
}

void DescriptorSetWriter::Write(
    VkDescriptorSet descriptorSet,
    uint32_t binding,
    VkDescriptorType descriptorType,
    uint32_t descriptorCount,
    VkWriteDescriptorSetAccelerationStructureKHR* pWriteAccelerationStructure) {
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = pWriteAccelerationStructure;
  write.dstSet = descriptorSet;
  write.dstBinding = binding;
  write.descriptorType = descriptorType;
  write.descriptorCount = descriptorCount;
  mWrites[index] = write;
  index++;
}

void DescriptorSetWriter::Update(VkDevice device) {
  vkUpdateDescriptorSets(device, static_cast<uint32_t>(mWrites.size()),
                         mWrites.data(), 0, VK_NULL_HANDLE);
}

}  // namespace hkr
