#include "glm/ext/matrix_transform.hpp"
#include "hikari/Util/Logger.h"

#include "Renderer/Renderer.h"
#include "Core/Math.h"
#include "Util/Assert.h"
#include "Util/vk_debug.h"
#include "Core/Window.h"

#include "Core/stb_image.h"
#include "Core/tiny_obj_loader.h"

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include <fmt/base.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <chrono>
#include <memory>
#include <vector>
#include <cstring>
#include <fstream>

namespace std {
template <> struct hash<hkr::Vertex> {
  size_t operator()(hkr::Vertex const& vertex) const {
    return ((hash<hkr::Vec3>()(vertex.pos) ^
             (hash<hkr::Vec3>()(vertex.color) << 1)) >>
            1) ^
           (hash<hkr::Vec2>()(vertex.texCoord) << 1);
  }
};

}  // namespace std

struct UniformBufferObject {
  alignas(16) hkr::Mat4 model;
  alignas(16) hkr::Mat4 view;
  alignas(16) hkr::Mat4 proj;
};

namespace {

VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice,
                                 VkFormat* depthFormat) {
  // Since all depth formats may be optional, we need to find a suitable depth
  // format to use Start with the highest precision packed format
  std::vector<VkFormat> formatList = {
      VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM};

  for (auto& format : formatList) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
    if (formatProps.optimalTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      *depthFormat = format;
      return true;
    }
  }

  return false;
}

VkBool32 getSupportedDepthStencilFormat(VkPhysicalDevice physicalDevice,
                                        VkFormat* depthStencilFormat) {
  std::vector<VkFormat> formatList = {
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT,
  };

  for (auto& format : formatList) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
    if (formatProps.optimalTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      *depthStencilFormat = format;
      return true;
    }
  }

  return false;
}

void insertImageMemoryBarrier(VkCommandBuffer cmdbuffer,
                              VkImage image,
                              VkAccessFlags srcAccessMask,
                              VkAccessFlags dstAccessMask,
                              VkImageLayout oldImageLayout,
                              VkImageLayout newImageLayout,
                              VkPipelineStageFlags srcStageMask,
                              VkPipelineStageFlags dstStageMask,
                              VkImageSubresourceRange subresourceRange) {
  VkImageMemoryBarrier imageMemoryBarrier{};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.srcAccessMask = srcAccessMask;
  imageMemoryBarrier.dstAccessMask = dstAccessMask;
  imageMemoryBarrier.oldLayout = oldImageLayout;
  imageMemoryBarrier.newLayout = newImageLayout;
  imageMemoryBarrier.image = image;
  imageMemoryBarrier.subresourceRange = subresourceRange;

  vkCmdPipelineBarrier(cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0,
                       nullptr, 1, &imageMemoryBarrier);
}

std::vector<char> ReadFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  HKR_ASSERT(file.is_open());

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* pUserData) {
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    HKR_INFO(pCallbackData->pMessage);
    // HKR_INFO("{} - {}: {}", pCallbackData->messageIdNumber,
    //          pCallbackData->pMessageIdName, pCallbackData->pMessage);
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    HKR_INFO(pCallbackData->pMessage);
    // HKR_INFO("{} - {}: {}", pCallbackData->messageIdNumber,
    //          pCallbackData->pMessageIdName, pCallbackData->pMessage);
  } else if (messageSeverity &
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    HKR_WARN(pCallbackData->pMessage);
    // HKR_WARN("{} - {}: {}", pCallbackData->messageIdNumber,
    //          pCallbackData->pMessageIdName, pCallbackData->pMessage);
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    HKR_ERROR(pCallbackData->pMessage);
    // HKR_ERROR("{} - {}: {}", pCallbackData->messageIdNumber,
    //           pCallbackData->pMessageIdName, pCallbackData->pMessage);
  }

  return VK_FALSE;
}

}  // namespace

namespace hkr {

void Renderer::Init(const AppSettings& settings, GLFWwindow* window) {
  mModelPath = settings.AssetPath;
  mModelPath += settings.ModelRelPath;
  mTexturePath = settings.AssetPath;
  mTexturePath += settings.TextureRelPath;
  mShaderPath = settings.AssetPath;
  mShaderPath += "spirv/";
  mAppName = settings.AppName;
  mWidth = settings.Width;
  mHeight = settings.Height;
  mVsync = settings.Vsync;
  mWindow = window;

  // instance, physical device, logical device, graphics queue
  InitVulkan();
  // swapchain, swapchain images, swapchain imageviews
  CreateSwapchain();

  // off-screen color, depth images
  CreateColorImage();
  CreateDepthImage();

  CreateCommandPool();
  CreateCommandBuffers();

  // descriptors resources
  CreateTextureImage();
  CreateTextureSampler();
  LoadModel();
  CreateVertexBuffer();
  CreateIndexBuffer();
  CreateUniformBuffers();

  CreateDescriptorSetLayout();
  CreateDescriptorPool();
  CreateDescriptorSets();

  CreateGraphicsPipeline();

  CreateSyncObjects();

  InitCamera();
}

Renderer::~Renderer() { CleanUp(); }

void Renderer::InitVulkan() {
  // 1. create instance
  vkb::InstanceBuilder instance_builder;
  instance_builder.set_app_name(mAppName)
      .set_engine_name("hikari engine")
      .require_api_version(1, 4, 0)
#ifdef ENABLE_VALIDATION_LAYER
      .request_validation_layers()
#endif
#ifdef HKR_DEBUG
      .set_debug_callback(&debugCallback);
#endif

  // query and enable instance extension
  // auto system_info_ret = vkb::SystemInfo::get_system_info();
  // HKR_ASSERT(system_info_ret);
  // auto system_info = system_info_ret.value();
  // if (system_info.is_extension_available(
  //         "VK_KHR_get_physical_device_properties2")) {
  //   instance_builder.enable_extension("VK_KHR_get_physical_device_properties2");
  // }
  // uint32_t glfwExtensionCount = 0;
  // const char** glfwExtensions = nullptr;
  // glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  // instance_builder.enable_extensions(glfwExtensionCount, glfwExtensions);

  auto inst_ret = instance_builder.build();
  HKR_ASSERT(inst_ret);
  vkb::Instance vkb_inst = inst_ret.value();
  mInstance = vkb_inst.instance;
  mDebugMessenger = vkb_inst.debug_messenger;

  VK_CHECK(glfwCreateWindowSurface(mInstance, mWindow, NULL, &mSurface));

  // 2. choose physical device
  vkb::PhysicalDeviceSelector selector{vkb_inst};
  selector.set_surface(mSurface)
      .set_minimum_version(1, 4)
      .require_dedicated_transfer_queue();

  // enable device extension
  //  Application cannot function without this extension
  // selector.add_required_extension("VK_KHR_timeline_semaphore");

  // core feature
  VkPhysicalDeviceFeatures required_features{};
  required_features.samplerAnisotropy = true;
  selector.set_required_features(required_features);
  // 1.2 feature
  VkPhysicalDeviceVulkan12Features features12{};
  features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  features12.bufferDeviceAddress = true;
  features12.descriptorIndexing = true;
  features12.descriptorBindingPartiallyBound = true;
  features12.descriptorBindingVariableDescriptorCount = true;
  features12.runtimeDescriptorArray = true;
  selector.set_required_features_12(features12);
  // 1.3 feature
  VkPhysicalDeviceVulkan13Features features13{};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.dynamicRendering = true;
  features13.synchronization2 = true;
  selector.set_required_features_13(features13);
  // extension feature
  // VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
  // descriptor_indexing_features.sType =
  //     VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
  // descriptor_indexing_features.descriptorBindingPartiallyBound = true;
  // selector.add_required_extension_features(descriptor_indexing_features);

  auto phys_ret = selector.select();
  HKR_ASSERT(phys_ret);
  vkb::PhysicalDevice physDevice = phys_ret.value();
  // bool supported =
  //     physDevice.enable_extension_if_present("VK_KHR_timeline_semaphore");
  mPhysDevice = physDevice.physical_device;

  // 3. create logical device
  vkb::DeviceBuilder device_builder{physDevice};
  auto dev_ret = device_builder.build();
  HKR_ASSERT(dev_ret);
  vkb::Device vkb_device = dev_ret.value();
  mDevice = vkb_device.device;

  // 4. get graphics queue
  auto graphics_queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);
  HKR_ASSERT(graphics_queue_ret);
  mGraphicsQueue = graphics_queue_ret.value();
  mGraphicsFamilyIndex =
      vkb_device.get_queue_index(vkb::QueueType::graphics).value();

  // 5. setup vma allocator
  // VmaVulkanFunctions vulkanFunctions{};
  // vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
  // vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
  allocatorInfo.physicalDevice = mPhysDevice;
  allocatorInfo.device = mDevice;
  allocatorInfo.instance = mInstance;
  // allocatorInfo.pVulkanFunctions = &vulkanFunctions;

  vmaCreateAllocator(&allocatorInfo, &mAllocator);
}  // namespace hkr

void Renderer::CreateSwapchain() {
  mSwapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
  vkb::SwapchainBuilder swapchain_builder{mPhysDevice, mDevice, mSurface};
  swapchain_builder
      .set_desired_format(
          VkSurfaceFormatKHR{.format = mSwapchainImageFormat,
                             .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
      .set_desired_extent(mWidth, mHeight)
      .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  if (mVsync) {
    swapchain_builder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR);
  } else {
    swapchain_builder.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR);
  }
  auto swap_ret = swapchain_builder.build();
  HKR_ASSERT(swap_ret);
  vkb::Swapchain swapchain = swap_ret.value();
  mSwapchain = swapchain.swapchain;
  mSwapchainImages = swapchain.get_images().value();
  mSwapchainImageViews = swapchain.get_image_views().value();
  VK_LABEL(VK_OBJECT_TYPE_IMAGE_VIEW, mSwapchainImageViews[0],
           "swapchain image view");
}

void Renderer::RecreateSwapchain() {
  vkDeviceWaitIdle(mDevice);
  // HKR_INFO("recreate swapchain with width: {}, height: {}", mWidth, mHeight);
  vkb::SwapchainBuilder swapchain_builder{mPhysDevice, mDevice, mSurface};
  swapchain_builder
      .set_desired_format(
          VkSurfaceFormatKHR{.format = mSwapchainImageFormat,
                             .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
      .set_old_swapchain(mSwapchain)
      .set_desired_extent(mWidth, mHeight)
      .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  if (mVsync) {
    swapchain_builder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR);
  } else {
    swapchain_builder.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR);
  }
  auto swap_ret = swapchain_builder.build();
  HKR_ASSERT(swap_ret);
  vkb::Swapchain swapchain = swap_ret.value();

  // clean up off-screen images and swapchain images
  vkDestroyImageView(mDevice, mColorImageView, nullptr);
  vkDestroyImageView(mDevice, mDepthImageView, nullptr);
  vmaDestroyImage(mAllocator, mColorImage, mColorImageAlloc);
  vmaDestroyImage(mAllocator, mDepthImage, mDepthImageAlloc);
  for (auto imageView : mSwapchainImageViews) {
    vkDestroyImageView(mDevice, imageView, nullptr);
  }
  vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

  // create new off-screen images and swapchain images
  mSwapchain = swapchain.swapchain;
  mSwapchainImages = swapchain.get_images().value();
  mSwapchainImageViews = swapchain.get_image_views().value();

  CreateColorImage();
  CreateDepthImage();
}

void Renderer::CreateColorImage() {
  CreateImage(mColorImage, mColorImageAlloc, mWidth, mHeight, 1,
              VK_SAMPLE_COUNT_1_BIT, mSwapchainImageFormat,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
              VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

  mColorImageView = CreateImageView(mColorImage, mSwapchainImageFormat,
                                    VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void Renderer::CreateDepthImage() {
  mDepthFormat = FindDepthFormat();

  CreateImage(mDepthImage, mDepthImageAlloc, mWidth, mHeight, 1,
              VK_SAMPLE_COUNT_1_BIT, mDepthFormat, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (mDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
    aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  mDepthImageView = CreateImageView(mDepthImage, mDepthFormat, aspectFlags, 1);
}

void Renderer::CreateImage(VkImage& image,
                           VmaAllocation& imageAlloc,
                           uint32_t width,
                           uint32_t height,
                           uint32_t mipLevels,
                           VkSampleCountFlagBits numSamples,
                           VkFormat format,
                           VkImageTiling tiling,
                           VkImageUsageFlags usage,
                           VmaAllocatorCreateFlags allocFlags) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = numSamples;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.flags = allocFlags;
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.priority = 1.0f;

  vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &image, &imageAlloc,
                 nullptr);
}

VkImageView Renderer::CreateImageView(VkImage image,
                                      VkFormat format,
                                      VkImageAspectFlags aspectFlags,
                                      uint32_t mipLevels) {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  // viewInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
  //                        VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  VkImageView imageView;
  VK_CHECK(vkCreateImageView(mDevice, &viewInfo, nullptr, &imageView));

  return imageView;
}

void Renderer::CreateCommandPool() {
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.pNext = nullptr;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = mGraphicsFamilyIndex;

  VK_CHECK(vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool));
}

void Renderer::CreateCommandBuffers() {
  mCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.pNext = nullptr;
  allocInfo.commandPool = mCommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());

  VK_CHECK(
      vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()));
}

void Renderer::CreateDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.pImmutableSamplers = nullptr;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 1;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.pImmutableSamplers = nullptr;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding,
                                                          samplerLayoutBinding};
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr,
                                       &mDescriptorSetLayout));
}

void Renderer::CreateTextureImage() {
  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load(mTexturePath.c_str(), &texWidth, &texHeight,
                              &texChannels, STBI_rgb_alpha);
  VkDeviceSize imageSize = texWidth * texHeight * 4;
  uint32_t mipLevels = static_cast<uint32_t>(std::floor(
                           std::log2(std::max(texWidth, texHeight)))) +
                       1;

  HKR_ASSERT(pixels);

  VkBuffer stagingBuffer;
  VmaAllocation stagingBufferAlloc;
  CreateBuffer(stagingBuffer, stagingBufferAlloc, imageSize,
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT);

  void* data;
  vmaMapMemory(mAllocator, stagingBufferAlloc, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vmaUnmapMemory(mAllocator, stagingBufferAlloc);

  stbi_image_free(pixels);

  CreateImage(mTextureImage, mTextureImageAlloc, texWidth, texHeight, mipLevels,
              VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  TransitionImageLayout(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
  CopyBufferToImage(stagingBuffer, mTextureImage,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));

  vmaDestroyBuffer(mAllocator, stagingBuffer, stagingBufferAlloc);

  // transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while
  // generating mipmaps
  GenerateMipmaps(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight,
                  mipLevels);

  mTextureImageView = CreateImageView(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB,
                                      VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
  VK_LABEL(VK_OBJECT_TYPE_IMAGE_VIEW, mTextureImageView, "texture image view");
}

void Renderer::CreateTextureSampler() {
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(mPhysDevice, &properties);

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
  samplerInfo.mipLodBias = 0.0f;

  VK_CHECK(vkCreateSampler(mDevice, &samplerInfo, nullptr, &mTextureSampler));
}

void Renderer::CreateDescriptorPool() {
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VK_CHECK(
      vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool));
}

void Renderer::CreateDescriptorSets() {
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                             mDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = mDescriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = layouts.data();

  mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

  VK_CHECK(
      vkAllocateDescriptorSets(mDevice, &allocInfo, mDescriptorSets.data()));

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = mUniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = mTextureSampler;
    imageInfo.imageView = mTextureImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = mDescriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = mDescriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(mDevice,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
  }
}

void Renderer::CreatePipelineCache() {
  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VK_CHECK(vkCreatePipelineCache(mDevice, &pipelineCacheCreateInfo, nullptr,
                                 &mPipelineCache));
}

VkShaderModule Renderer::CreateShaderModule(const std::vector<char>& code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shaderModule;
  VK_CHECK(vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule));

  return shaderModule;
}

void Renderer::CreateGraphicsPipeline() {
  auto vertShaderCode = ReadFile(mShaderPath + "shader.vert.spv");
  auto fragShaderCode = ReadFile(mShaderPath + "shader.frag.spv");

  VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
      vertShaderStageInfo, fragShaderStageInfo};

  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.sampleShadingEnable = VK_FALSE;
  // multisampling.minSampleShading = .2f;
  // multisampling.rasterizationSamples = mMsaaSamples;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE,
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = depthStencil.back,
  depthStencil.back.failOp = VK_STENCIL_OP_KEEP;
  depthStencil.back.passOp = VK_STENCIL_OP_KEEP;
  depthStencil.back.compareOp = VK_COMPARE_OP_ALWAYS;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;

  VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr,
                                  &mPipelineLayout));

  VkPipelineRenderingCreateInfoKHR pipelineRenderingCI{};
  pipelineRenderingCI.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  pipelineRenderingCI.colorAttachmentCount = 1;
  pipelineRenderingCI.pColorAttachmentFormats = &mSwapchainImageFormat;
  pipelineRenderingCI.depthAttachmentFormat = mDepthFormat;
  if (mRequireStencil) {
    pipelineRenderingCI.stencilAttachmentFormat = mDepthFormat;
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = &pipelineRenderingCI;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = mPipelineLayout;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  // .renderPass = mRenderPass,
  // .subpass = 0,

  VK_CHECK(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo,
                                     nullptr, &mGraphicsPipeline));

  vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
}

void Renderer::LoadModel() {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                                 mModelPath.c_str());
  HKR_ASSERT(result);

  std::unordered_map<Vertex, uint32_t> uniqueVertices{};

  for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
      Vertex vertex{};

      vertex.pos = {attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};

      vertex.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                         1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};

      vertex.color = {1.0f, 1.0f, 1.0f};

      if (uniqueVertices.count(vertex) == 0) {
        uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
        mVertices.push_back(vertex);
      }

      mIndices.push_back(uniqueVertices[vertex]);
    }
  }
}

void Renderer::CreateVertexBuffer() {
  VkDeviceSize bufferSize = sizeof(mVertices[0]) * mVertices.size();

  VkBuffer stagingBuffer;
  VmaAllocation stagingBufferAlloc;
  CreateBuffer(stagingBuffer, stagingBufferAlloc, bufferSize,
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT);

  void* data;
  vmaMapMemory(mAllocator, stagingBufferAlloc, &data);
  memcpy(data, mVertices.data(), (size_t)bufferSize);
  vmaUnmapMemory(mAllocator, stagingBufferAlloc);

  CreateBuffer(
      mVertexBuffer, mVertexBufferAlloc, bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  CopyBuffer(stagingBuffer, mVertexBuffer, bufferSize);

  vmaDestroyBuffer(mAllocator, stagingBuffer, stagingBufferAlloc);
}

void Renderer::CreateIndexBuffer() {
  VkDeviceSize bufferSize = sizeof(mIndices[0]) * mIndices.size();

  VkBuffer stagingBuffer;
  VmaAllocation stagingBufferAlloc;
  CreateBuffer(stagingBuffer, stagingBufferAlloc, bufferSize,
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT);

  void* data;
  vmaMapMemory(mAllocator, stagingBufferAlloc, &data);
  memcpy(data, mIndices.data(), (size_t)bufferSize);
  vmaUnmapMemory(mAllocator, stagingBufferAlloc);

  CreateBuffer(
      mIndexBuffer, mIndexBufferAlloc, bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  CopyBuffer(stagingBuffer, mIndexBuffer, bufferSize);

  vmaDestroyBuffer(mAllocator, stagingBuffer, stagingBufferAlloc);
}

void Renderer::CreateUniformBuffers() {
  VkDeviceSize bufferSize = sizeof(UniformBufferObject);

  mUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  mUniformBuffersAlloc.resize(MAX_FRAMES_IN_FLIGHT);
  mUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    CreateBuffer(
        mUniformBuffers[i], mUniformBuffersAlloc[i], bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);

    vmaMapMemory(mAllocator, mUniformBuffersAlloc[i],
                 &mUniformBuffersMapped[i]);
  }
}

void Renderer::CreateSyncObjects() {
  mImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  mRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  mInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr,
                               &mImageAvailableSemaphores[i]));
    VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr,
                               &mRenderFinishedSemaphores[i]));
    VK_CHECK(vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]));
  }
}

// VkSampleCountFlagBits Renderer::GetMaxUsableSampleCount() {
//   VkPhysicalDeviceProperties physicalDeviceProperties;
//   vkGetPhysicalDeviceProperties(mPhysicalDevice,
//   &physicalDeviceProperties);
//
//   VkSampleCountFlags counts =
//       physicalDeviceProperties.limits.framebufferColorSampleCounts &
//       physicalDeviceProperties.limits.framebufferDepthSampleCounts;
//   if (counts & VK_SAMPLE_COUNT_64_BIT) {
//     return VK_SAMPLE_COUNT_64_BIT;
//   }
//   if (counts & VK_SAMPLE_COUNT_32_BIT) {
//     return VK_SAMPLE_COUNT_32_BIT;
//   }
//   if (counts & VK_SAMPLE_COUNT_16_BIT) {
//     return VK_SAMPLE_COUNT_16_BIT;
//   }
//   if (counts & VK_SAMPLE_COUNT_8_BIT) {
//     return VK_SAMPLE_COUNT_8_BIT;
//   }
//   if (counts & VK_SAMPLE_COUNT_4_BIT) {
//     return VK_SAMPLE_COUNT_4_BIT;
//   }
//   if (counts & VK_SAMPLE_COUNT_2_BIT) {
//     return VK_SAMPLE_COUNT_2_BIT;
//   }
//
//   return VK_SAMPLE_COUNT_1_BIT;
// }

VkFormat Renderer::FindSupportedFormat(const std::vector<VkFormat>& candidates,
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

VkFormat Renderer::FindDepthFormat() {
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

void Renderer::CreateBuffer(VkBuffer& buffer,
                            VmaAllocation& alloc,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VmaAllocationCreateFlags allocFlags) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.flags = allocFlags;
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

  VmaAllocationInfo allocInfo;
  vmaCreateBuffer(mAllocator, &bufferInfo, &allocCreateInfo, &buffer, &alloc,
                  &allocInfo);
}  // namespace hkr

void Renderer::TransitionImageLayout(VkImage image,
                                     VkFormat format,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     uint32_t mipLevels) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    HKR_ERROR("");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  EndSingleTimeCommands(commandBuffer);
}

void Renderer::CopyBufferToImage(VkBuffer buffer,
                                 VkImage image,
                                 uint32_t width,
                                 uint32_t height) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  EndSingleTimeCommands(commandBuffer);
}

VkCommandBuffer Renderer::BeginSingleTimeCommands() {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = mCommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void Renderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(mGraphicsQueue);

  vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
}

void Renderer::GenerateMipmaps(VkImage image,
                               VkFormat imageFormat,
                               int32_t texWidth,
                               int32_t texHeight,
                               uint32_t mipLevels) {
  // Check if image format supports linear blitting
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(mPhysDevice, imageFormat,
                                      &formatProperties);

  HKR_ASSERT(formatProperties.optimalTilingFeatures &
             VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  int32_t mipWidth = texWidth;
  int32_t mipHeight = texHeight;

  for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                          mipHeight > 1 ? mipHeight / 2 : 1, 1};

    vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    if (mipWidth > 1) mipWidth /= 2;
    if (mipHeight > 1) mipHeight /= 2;
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  EndSingleTimeCommands(commandBuffer);
}

void Renderer::CopyBuffer(VkBuffer srcBuffer,
                          VkBuffer dstBuffer,
                          VkDeviceSize size) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  EndSingleTimeCommands(commandBuffer);
}

void Renderer::DrawFrame() {
  if (mFramebufferResized) {
    mFramebufferResized = false;
    RecreateSwapchain();
    return;
  }

  vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE,
                  UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      mDevice, mSwapchain, UINT64_MAX, mImageAvailableSemaphores[mCurrentFrame],
      VK_NULL_HANDLE, &imageIndex);

  // if (result == VK_ERROR_OUT_OF_DATE_KHR || mFramebufferResized) {
  //   mFramebufferResized = false;
  //   RecreateSwapchain();
  //   return;
  // }
  HKR_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);

  UpdateUniformBuffer(mCurrentFrame);

  vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

  vkResetCommandBuffer(mCommandBuffers[mCurrentFrame],
                       /*VkCommandBufferResetFlagBits*/ 0);
  RecordCommandBuffer(mCommandBuffers[mCurrentFrame], imageIndex);

  // Pipeline stage at which the queue submission will wait (via
  // pWaitSemaphores)
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore waitSemaphores[] = {mImageAvailableSemaphores[mCurrentFrame]};
  VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphores[mCurrentFrame]};
  // The submit info structure specifies a command buffer queue submission
  // batch
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &mCommandBuffers[mCurrentFrame];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK(vkQueueSubmit(mGraphicsQueue, 1, &submitInfo,
                         mInFlightFences[mCurrentFrame]));

  VkSwapchainKHR swapChains[] = {mSwapchain};
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(mGraphicsQueue, &presentInfo);

  // if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
  //     mFramebufferResized) {
  //   mFramebufferResized = false;
  //   RecreateSwapchain();
  // }
  HKR_ASSERT(result == VK_SUCCESS);

  mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::UpdateUniformBuffer(uint32_t currentImage) {
  // static auto startTime = std::chrono::high_resolution_clock::now();
  //
  // auto currentTime = std::chrono::high_resolution_clock::now();
  // float time = std::chrono::duration<float, std::chrono::seconds::period>(
  //                  currentTime - startTime)
  //                  .count();

  UniformBufferObject ubo{};
  // ubo.model = rotate(mat4(1.0f), time * radians(90.0f),
  //                         vec3(0.0f, 0.0f, 1.0f));
  // ubo.model = mat4(1.0f);
  // ubo.view = glm::lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f),
  //                        vec3(0.0f, 0.0f, 1.0f));
  // ubo.proj = glm::perspective(
  //     glm::radians(45.0f),
  //     mSwapChainExtent.width / (float)mSwapChainExtent.height,
  //     0.1f, 10.0f);
  // ubo.proj[1][1] *= -1;

  ubo.model = Mat4(1.0);
  // ubo.view = glm::lookAt(Vec3(2.0f, 0.0f, 2.0f), Vec3(0.0f, 0.0f, 0.0f),
  //                        Vec3(0.0f, 1.0f, 0.0f));
  // ubo.proj = glm::perspective(glm::radians(45.0f), mWidth / (float)mHeight,
  //                             0.1f, 10.0f);
  // ubo.proj[1][1] *= -1;

  // ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
  //                         glm::vec3(0.0f, 1.0f, 0.0f));
  // ubo.view = mCamera.matrices.view;
  // ubo.proj = mCamera.matrices.perspective;
  // ubo.proj[1][1] *= -1;

  ubo.view = mCamera.GetView();
  ubo.proj = mCamera.GetProj();

  memcpy(mUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Renderer::RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                   uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

  insertImageMemoryBarrier(
      commandBuffer, mSwapchainImages[imageIndex], 0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (mRequireStencil) {
    aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  insertImageMemoryBarrier(commandBuffer, mDepthImage, 0,
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                               VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                               VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                           VkImageSubresourceRange{aspectMask, 0, 1, 0, 1});

  // Color attachment
  VkRenderingAttachmentInfo colorAttachment{
      VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  colorAttachment.imageView = mSwapchainImageViews[imageIndex];
  colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.clearValue.color = {0.2f, 0.3f, 0.3f, 0.0f};
  // Depth/stencil attachment
  VkRenderingAttachmentInfo depthStencilAttachment{
      VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  depthStencilAttachment.imageView = mDepthImageView;
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

  // std::array<VkClearValue, 2> clearValues{};
  // clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  // clearValues[1].depthStencil = {1.0f, 0};

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

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mPipelineLayout, 0, 1,
                          &mDescriptorSets[mCurrentFrame], 0, nullptr);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    mGraphicsPipeline);

  VkBuffer vertexBuffers[] = {mVertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

  vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mIndices.size()), 1, 0,
                   0, 0);

  vkCmdEndRendering(commandBuffer);
  // This barrier prepares the color image for presentation, we don't need to
  // care for the depth image
  insertImageMemoryBarrier(
      commandBuffer, mSwapchainImages[imageIndex],
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
      VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void Renderer::Render() {
  auto tStart = std::chrono::high_resolution_clock::now();

  DrawFrame();

  auto tEnd = std::chrono::high_resolution_clock::now();
  auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
  float frameTimer = tDiff / 1000.0f;
  mCamera.Update(frameTimer);
}

void Renderer::CleanUp() {
  vkDeviceWaitIdle(mDevice);

  vkDestroyImageView(mDevice, mColorImageView, nullptr);
  vmaDestroyImage(mAllocator, mColorImage, mColorImageAlloc);
  vkDestroyImageView(mDevice, mDepthImageView, nullptr);
  vmaDestroyImage(mAllocator, mDepthImage, mDepthImageAlloc);
  for (auto imageView : mSwapchainImageViews) {
    vkDestroyImageView(mDevice, imageView, nullptr);
  }
  vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

  vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
  vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vmaUnmapMemory(mAllocator, mUniformBuffersAlloc[i]);
    vmaDestroyBuffer(mAllocator, mUniformBuffers[i], mUniformBuffersAlloc[i]);
  }

  vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
  //
  vkDestroySampler(mDevice, mTextureSampler, nullptr);
  vkDestroyImageView(mDevice, mTextureImageView, nullptr);
  vmaDestroyImage(mAllocator, mTextureImage, mTextureImageAlloc);
  vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);

  vmaDestroyBuffer(mAllocator, mVertexBuffer, mVertexBufferAlloc);
  vmaDestroyBuffer(mAllocator, mIndexBuffer, mIndexBufferAlloc);

  vmaDestroyAllocator(mAllocator);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
    vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
    vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
  }

  vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
  vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

  vkDestroyDevice(mDevice, nullptr);

#ifdef ENABLE_VALIDATION_LAYER
  auto vkDestroyDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
          vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT"));
  if (vkDestroyDebugUtilsMessengerEXT != nullptr) {
    vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
  }
#endif

  vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
  vkDestroyInstance(mInstance, nullptr);
}

void Renderer::InitCamera() {
  mCamera.SetMoveSpeed(1.0f);
  mCamera.SetRotateSpeed(0.2f);
  mCamera.SetPerspective(60.0f, (float)mWidth / (float)mHeight, 0.1f, 256.0f);

  // mCamera.SetRotation({-7.75f, 150.25f, 0.0f});
  // mCamera.SetPosition({0.7f, 0.1f, 1.7f});
}

void Renderer::OnKeyEvent(int key, int action) {
  switch (key) {
    case GLFW_KEY_W:
      mCamera.State.Up = !!action;
      break;
    case GLFW_KEY_S:
      mCamera.State.Down = !!action;
      break;
    case GLFW_KEY_A:
      mCamera.State.Left = !!action;
      break;
    case GLFW_KEY_D:
      mCamera.State.Right = !!action;
      break;
    case GLFW_KEY_J:
      mCamera.State.Descend = !!action;
      break;
    case GLFW_KEY_K:
      mCamera.State.Ascend = !!action;
      break;
    default:
      break;
  }
}

void Renderer::OnMouseEvent(int button, int action) {
  switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
      mMouse.State.Left = !!action;
      break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
      mMouse.State.Middle = !!action;
      break;
    case GLFW_MOUSE_BUTTON_RIGHT:
      mMouse.State.Right = !!action;
      break;
    default:
      break;
  }
}

void Renderer::OnMouseMoveEvent(double x, double y) {
  float oldX = mMouse.Position.x;
  float oldY = mMouse.Position.y;
  float dx = x - oldX;
  float dy = y - oldY;
  dy *= -1;
  mMouse.Position = {x, y};
  if (mMouse.State.Left) {
    mCamera.Rotate(dy, dx, 0);
  }

  if (mMouse.State.Right) {
    mCamera.Translate(-0.0f, 0.0f, dy * .005f);
  }
  if (mMouse.State.Middle) {
    mCamera.Translate(-dx * 0.005f, -dy * 0.005f, 0.0f);
  }
}

void Renderer::Resize(int width, int height) {
  mWidth = width;
  mHeight = height;
  mCamera.SetAspect((float)mWidth / (float)mHeight);
  mFramebufferResized = true;
}

}  // namespace hkr
