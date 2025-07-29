#pragma once

#include <volk.h>

#include <cstdint>
#include <vector>

namespace hkr {

class DescriptorSetLayoutBuilder {
public:
  DescriptorSetLayoutBuilder(uint32_t bindingCount) : mBindings(bindingCount) {}
  void AddBinding(uint32_t binding,
                  VkDescriptorType descriptorType,
                  VkShaderStageFlags stageFlags,
                  uint32_t descriptorCount = 1);

  VkDescriptorSetLayout Build(VkDevice device, bool variableDescriptor = false);

private:
  std::vector<VkDescriptorSetLayoutBinding> mBindings;
};

class DescriptorSetWriter {
public:
  DescriptorSetWriter(uint32_t bindingCount) : mWrites(bindingCount) {}
  void Write(VkDescriptorSet descriptorSet,
             uint32_t binding,
             VkDescriptorType descriptorType,
             uint32_t descriptorCount,
             VkDescriptorBufferInfo* pBufferInfo);
  void Write(VkDescriptorSet descriptorSet,
             uint32_t binding,
             VkDescriptorType descriptorType,
             uint32_t descriptorCount,
             VkDescriptorImageInfo* pImageInfo);
  void Write(VkDescriptorSet descriptorSet,
             uint32_t binding,
             VkDescriptorType descriptorType,
             uint32_t descriptorCount,
             VkWriteDescriptorSetAccelerationStructureKHR*
                 pWriteAccelerationStructure);
  void Update(VkDevice device);

private:
  uint32_t index = 0;
  std::vector<VkWriteDescriptorSet> mWrites;
};

}  // namespace hkr
