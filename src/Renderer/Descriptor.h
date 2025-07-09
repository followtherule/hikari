#pragma once

#include "Util/vk_debug.h"

#include <volk.h>

#include <cstdint>

namespace hkr {

template <uint32_t bindingCount> class DescriptorSetLayoutBuilder {
public:
  void AddBinding(uint32_t descriptorCount,
                  VkDescriptorType descriptorType,
                  VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = mBindingCount;
    binding.descriptorCount = descriptorCount;
    binding.descriptorType = descriptorType;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = stageFlags;
    mBindings[mBindingCount] = binding;
    mBindingCount++;
  }

  VkDescriptorSetLayout Build(VkDevice device) {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = mBindingCount;
    layoutInfo.pBindings = mBindings.data();

    VK_CHECK(
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout));
    return layout;
  }

private:
  uint32_t mBindingCount = 0;
  std::array<VkDescriptorSetLayoutBinding, bindingCount> mBindings;
};

}  // namespace hkr
