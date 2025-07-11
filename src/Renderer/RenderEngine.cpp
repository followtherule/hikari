#include "Renderer/Buffer.h"
#include "Renderer/Common.h"
#include "Renderer/Descriptor.h"
#include "Renderer/RenderEngine.h"
#include "Renderer/Rasterizer.h"
#include "hikari/Util/Logger.h"
#include "Core/Math.h"
#include "Util/Assert.h"
#include "Util/vk_debug.h"
#include "Util/vk_util.h"

#include "Core/stb_image.h"
#include "Core/tiny_obj_loader.h"

#include <volk.h>
#include <vk_mem_alloc.h>
#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <chrono>
#include <vector>
#include <cstring>

namespace {

void ImGuiCheck(VkResult err) {
  if (err == VK_SUCCESS) return;
  HKR_ERROR("[vulkan] Error: VkResult = {}", (int)err);
  if (err < 0) abort();
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* pUserData) {
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    HKR_INFO(pCallbackData->pMessage);
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    HKR_INFO(pCallbackData->pMessage);
  } else if (messageSeverity &
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    HKR_WARN(pCallbackData->pMessage);
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    HKR_ERROR(pCallbackData->pMessage);
  }

  return VK_FALSE;
}

}  // namespace

namespace hkr {

void RenderEngine::Init(const AppSettings& settings, GLFWwindow* window) {
  mModelPath = settings.assetPath;
  mModelPath += settings.modelRelPath;
  mTexturePath = settings.assetPath;
  mTexturePath += settings.textureRelPath;
  mShaderPath = settings.assetPath;
  mShaderPath += "spirv/";
  mAppName = settings.appName;
  mWidth = settings.width;
  mHeight = settings.height;
  mVsync = settings.vsync;
  mWindow = window;

  // instance, physical device, logical device, graphics queue
  InitVulkan();
  // swapchain, swapchain images, swapchain imageviews
  CreateSwapchain();

  CreateCommandPool();
  CreateCommandBuffers();

  CreateUniformBuffers();

  mRasterizer = new Rasterizer;
  mRasterizer->Init(mDevice, mPhysDevice, mGraphicsQueue, mCommandPool,
                    mUniformBuffers, mAllocator, mSwapchainImageFormat, mWidth,
                    mHeight, mModelPath, mTexturePath, mShaderPath);
  // mRaytracer->Init();

  CreateSyncObjects();

  InitImGui();

  InitCamera();
}

void RenderEngine::InitVulkan() {
  VK_CHECK(volkInitialize());

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
  auto system_info_ret = vkb::SystemInfo::get_system_info();
  HKR_ASSERT(system_info_ret);
  auto system_info = system_info_ret.value();
  if (system_info.is_extension_available(
          VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
    instance_builder.enable_extension(
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  }
  if (system_info.is_extension_available(
          VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    instance_builder.enable_extension(
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  }
  // uint32_t glfwExtensionCount = 0;
  // const char** glfwExtensions = nullptr;
  // glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  // instance_builder.enable_extensions(glfwExtensionCount, glfwExtensions);

  auto inst_ret = instance_builder.build();
  HKR_ASSERT(inst_ret);
  vkb::Instance vkb_inst = inst_ret.value();
  mInstance = vkb_inst.instance;
  volkLoadInstance(mInstance);
  mDebugMessenger = vkb_inst.debug_messenger;

  VK_CHECK(glfwCreateWindowSurface(mInstance, mWindow, NULL, &mSurface));

  // 2. choose physical device
  vkb::PhysicalDeviceSelector selector{vkb_inst};
  selector.set_surface(mSurface)
      .set_minimum_version(1, 4)
      .require_dedicated_transfer_queue();

  // enable device extension
  //  Application cannot function without this extension
  // device extensions required by raytracing
  selector.add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
  selector.add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
  // required by VK_KHR_acceleration_structure
  selector.add_required_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
  selector.add_required_extension(
      VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  selector.add_required_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
  // required for VK_KHR_ray_tracing_pipeline
  selector.add_required_extension(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

  // Required by VK_KHR_spirv_1_4
  selector.add_required_extension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

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
  volkLoadDevice(mDevice);

  // 4. get graphics queue
  auto graphics_queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);
  HKR_ASSERT(graphics_queue_ret);
  mGraphicsQueue = graphics_queue_ret.value();
  mGraphicsFamilyIndex =
      vkb_device.get_queue_index(vkb::QueueType::graphics).value();

  // 5. setup vma allocator
  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
  allocatorInfo.physicalDevice = mPhysDevice;
  allocatorInfo.device = mDevice;
  allocatorInfo.instance = mInstance;
  VmaVulkanFunctions vulkanFunctions{};
  VK_CHECK(vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vulkanFunctions));
  allocatorInfo.pVulkanFunctions = &vulkanFunctions;

  VK_CHECK(vmaCreateAllocator(&allocatorInfo, &mAllocator));
}  // namespace hkr

void RenderEngine::CreateSwapchain() {
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
}

void RenderEngine::RecreateSwapchain() {
  vkDeviceWaitIdle(mDevice);
  // HKR_INFO("recreate swapchain with width: {}, height: {}", mWidth,
  // mHeight);
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

  mRasterizer->OnResize(mWidth, mHeight);
  for (auto imageView : mSwapchainImageViews) {
    vkDestroyImageView(mDevice, imageView, nullptr);
  }
  vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

  // create new off-screen images and swapchain images
  mSwapchain = swapchain.swapchain;
  mSwapchainImages = swapchain.get_images().value();
  mSwapchainImageViews = swapchain.get_image_views().value();
}

void RenderEngine::CreateCommandPool() {
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.pNext = nullptr;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = mGraphicsFamilyIndex;

  VK_CHECK(vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool));
}

void RenderEngine::CreateCommandBuffers() {
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

void RenderEngine::CreateUniformBuffers() {
  VkDeviceSize bufferSize = sizeof(UniformBufferObject);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    mUniformBuffers[i].Create(mAllocator, bufferSize);
    // map buffer for the whole app lifetime
    mUniformBuffers[i].Map(mAllocator);
  }
}

void RenderEngine::CreateSyncObjects() {
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

void RenderEngine::InitImGui() {
  // If you wish to load e.g. additional textures you may need to alter pools
  // sizes and maxSets.
  {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 0;
    for (VkDescriptorPoolSize& pool_size : pool_sizes)
      pool_info.maxSets += pool_size.descriptorCount;
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    VK_CHECK(vkCreateDescriptorPool(mDevice, &pool_info, nullptr,
                                    &mImGuiDescriptorPool));
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad;             // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport
                                                       // / Platform Windows
  // io.ConfigViewportsNoAutoMerge = true;
  // io.ConfigViewportsNoTaskBarIcon = true;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // When viewports are enabled we tweak WindowRounding/WindowBg so platform
  // windows can look identical to regular ones.
  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan(mWindow, true);
  ImGui_ImplVulkan_InitInfo init_info{};
  // init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your
  // value of VkApplicationInfo::apiVersion, otherwise will default to header
  // version.
  init_info.Instance = mInstance;
  init_info.PhysicalDevice = mPhysDevice;
  init_info.Device = mDevice;
  init_info.QueueFamily = mGraphicsFamilyIndex;
  init_info.Queue = mGraphicsQueue;
  // init_info.PipelineCache = mPipelineCache;
  init_info.DescriptorPool = mImGuiDescriptorPool;

  VkPipelineRenderingCreateInfoKHR pipelineRenderingCI{};
  pipelineRenderingCI.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  pipelineRenderingCI.colorAttachmentCount = 1;
  pipelineRenderingCI.pColorAttachmentFormats = &mSwapchainImageFormat;
  // pipelineRenderingCI.depthAttachmentFormat = FindDepthFormat();
  // if (mRequireStencil) {
  //   pipelineRenderingCI.stencilAttachmentFormat = FindDepthFormat();
  // }
  init_info.UseDynamicRendering = true;
  init_info.PipelineRenderingCreateInfo = pipelineRenderingCI;
  init_info.MinImageCount = static_cast<uint32_t>(mSwapchainImages.size());
  init_info.ImageCount = static_cast<uint32_t>(mSwapchainImages.size());
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator = nullptr;
  init_info.CheckVkResultFn = ImGuiCheck;
  ImGui_ImplVulkan_Init(&init_info);

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can
  // also load multiple fonts and use ImGui::PushFont()/PopFont() to select
  // them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you
  // need to select the font among multiple.
  // - If the file cannot be loaded, the function will return a nullptr.
  // Please handle those errors in your application (e.g. use an assertion, or
  // display an error and quit).
  // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use
  // Freetype for higher quality font rendering.
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string
  // literal you need to write a double backslash \\ !
  // style.FontSizeBase = 20.0f;
  // io.Fonts->AddFontDefault();
  // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
  // ImFont* font =
  // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
  // IM_ASSERT(font != nullptr);
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

void RenderEngine::DrawFrame() {
  if (mFramebufferResized) {
    mFramebufferResized = false;
    RecreateSwapchain();
    ImGui_ImplVulkan_SetMinImageCount(
        static_cast<uint32_t>(mSwapchainImages.size()));
    return;
  }

  vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE,
                  UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      mDevice, mSwapchain, UINT64_MAX, mImageAvailableSemaphores[mCurrentFrame],
      VK_NULL_HANDLE, &imageIndex);
  HKR_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);

  UpdateUniformBuffer(mCurrentFrame);

  vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

  vkResetCommandBuffer(mCommandBuffers[mCurrentFrame],
                       /*VkCommandBufferResetFlagBits*/ 0);
  RecordCommandBuffer(mCommandBuffers[mCurrentFrame], mCurrentFrame,
                      imageIndex);

  VkCommandBufferSubmitInfo commandInfo{};
  commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  commandInfo.pNext = nullptr;
  commandInfo.commandBuffer = mCommandBuffers[mCurrentFrame];
  commandInfo.deviceMask = 0;

  VkSemaphoreSubmitInfo waitInfo{};
  waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  waitInfo.semaphore = mImageAvailableSemaphores[mCurrentFrame];
  waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
  waitInfo.deviceIndex = 0;
  waitInfo.value = 1;

  VkSemaphoreSubmitInfo signalInfo{};
  signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signalInfo.semaphore = mRenderFinishedSemaphores[mCurrentFrame];
  signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
  signalInfo.deviceIndex = 0;
  signalInfo.value = 1;

  // The submit info structure specifies a command buffer queue submission
  // batch
  VkSubmitInfo2 submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submitInfo.waitSemaphoreInfoCount = 1;
  submitInfo.pWaitSemaphoreInfos = &waitInfo;
  submitInfo.signalSemaphoreInfoCount = 1;
  submitInfo.pSignalSemaphoreInfos = &signalInfo;
  submitInfo.commandBufferInfoCount = 1;
  submitInfo.pCommandBufferInfos = &commandInfo;

  VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submitInfo,
                          mInFlightFences[mCurrentFrame]));

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &mRenderFinishedSemaphores[mCurrentFrame];
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &mSwapchain;
  presentInfo.pImageIndices = &imageIndex;

  VK_CHECK(vkQueuePresentKHR(mGraphicsQueue, &presentInfo));

  mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void RenderEngine::UpdateUniformBuffer(uint32_t currentImage) {
  UniformBufferObject ubo{};
  ubo.model = Mat4(1.0);
  ubo.model = glm::rotate(ubo.model, glm::radians(-90.0f), {0, 1, 0});
  ubo.model = glm::rotate(ubo.model, glm::radians(-90.0f), {1, 0, 0});

  ubo.view = mCamera.GetView();
  ubo.proj = mCamera.GetProj();

  mUniformBuffers[currentImage].Write(&ubo, sizeof(ubo));
}

void RenderEngine::RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                       uint32_t currentFrame,
                                       uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
  mRasterizer->RecordCommandBuffer(commandBuffer, currentFrame,
                                   mSwapchainImages[imageIndex]);
  InsertImageMemoryBarrier(
      commandBuffer, mSwapchainImages[imageIndex],
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  DrawImGui(commandBuffer, imageIndex);
  InsertImageMemoryBarrier(
      commandBuffer, mSwapchainImages[imageIndex],
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
      VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void RenderEngine::Render() {
  auto tStart = std::chrono::high_resolution_clock::now();

  DrawFrame();

  auto tEnd = std::chrono::high_resolution_clock::now();
  auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
  float frameTimer = tDiff / 1000.0f;
  mCamera.Update(frameTimer);
}

void RenderEngine::DrawImGui(VkCommandBuffer commandBuffer,
                             uint32_t imageIndex) {
  // Start the Dear ImGui frame
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // 1. Show the big demo window (Most of the sample code is in
  // ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
  // ImGui!).

  static bool show_demo_window = true;
  static bool show_another_window = true;
  static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

  // 2. Show a simple window that we create ourselves. We use a Begin/End pair
  // to create a named window.
  {
    static float f = 0.0f;
    static int counter = 0;

    ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!"
                                    // and append into it.

    ImGui::Text("This is some useful text.");  // Display some text (you can
                                               // use a format strings too)
    ImGui::Checkbox(
        "Demo Window",
        &show_demo_window);  // Edit bools storing our window open/close state
    ImGui::Checkbox("Another Window", &show_another_window);

    ImGui::SliderFloat("float", &f, 0.0f,
                       1.0f);  // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::ColorEdit3(
        "clear color",
        (float*)&clear_color);  // Edit 3 floats representing a color

    if (ImGui::Button("Button"))  // Buttons return true when clicked (most
                                  // widgets return true when edited/activated)
      counter++;
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
  }

  // 3. Show another simple window.
  if (show_another_window) {
    ImGui::Begin(
        "Another Window",
        &show_another_window);  // Pass a pointer to our bool variable (the
                                // window will have a closing button that will
                                // clear the bool when clicked)
    ImGui::Text("Hello from another window!");
    if (ImGui::Button("Close Me")) show_another_window = false;
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
  ImDrawData* main_draw_data = ImGui::GetDrawData();
  const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f ||
                                  main_draw_data->DisplaySize.y <= 0.0f);
  if (!main_is_minimized) {
    // Color attachment
    VkRenderingAttachmentInfo colorAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = mSwapchainImageViews[imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = {static_cast<uint32_t>(mWidth),
                                       static_cast<uint32_t>(mHeight)};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

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

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(main_draw_data, commandBuffer);

    vkCmdEndRendering(commandBuffer);
  }

  // Update and Render additional Platform Windows
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

void RenderEngine::CleanupImGui() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void RenderEngine::Cleanup() {
  vkDeviceWaitIdle(mDevice);

  CleanupImGui();

  mRasterizer->Cleanup();
  delete mRasterizer;
  vkDestroyDescriptorPool(mDevice, mImGuiDescriptorPool, nullptr);
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    mUniformBuffers[i].Unmap(mAllocator);
    mUniformBuffers[i].Cleanup(mAllocator);
  }

  for (auto imageView : mSwapchainImageViews) {
    vkDestroyImageView(mDevice, imageView, nullptr);
  }
  vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

  vmaDestroyAllocator(mAllocator);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
    vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
    vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
  }

  vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

  vkDestroyDevice(mDevice, nullptr);

#ifdef ENABLE_VALIDATION_LAYER
  if (vkDestroyDebugUtilsMessengerEXT != nullptr) {
    vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
  }
#endif

  vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
  vkDestroyInstance(mInstance, nullptr);
}

void RenderEngine::InitCamera() {
  mCamera.SetMoveSpeed(1.0f);
  mCamera.SetRotateSpeed(0.2f);
  mCamera.SetPerspective(60.0f, (float)mWidth / (float)mHeight, 0.1f, 256.0f);

  // mCamera.SetRotation({-7.75f, 150.25f, 0.0f});
  // mCamera.SetPosition({0.7f, 0.1f, 1.7f});
}

void RenderEngine::OnKeyEvent(int key, int action) {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureKeyboard) {
    return;
  }
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

void RenderEngine::OnMouseEvent(int button, int action) {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    return;
  }
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

void RenderEngine::OnMouseMoveEvent(double x, double y) {
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
    mCamera.Translate(0.0f, 0.0f, dy * .005f);
  }
  if (mMouse.State.Middle) {
    mCamera.Translate(-dx * 0.005f, -dy * 0.005f, 0.0f);
  }
}

void RenderEngine::OnResize(int width, int height) {
  mWidth = width;
  mHeight = height;
  mCamera.SetAspect((float)mWidth / (float)mHeight);
  mFramebufferResized = true;
}

}  // namespace hkr
