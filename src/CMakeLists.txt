include(GenerateExportHeader)

add_library(
  hikari
  SHARED
  Core/App.cpp
  Core/Window.cpp
  Core/stb_image.cpp
  Core/tiny_obj_loader.cpp

  Renderer/Buffer.cpp
  Renderer/Descriptor.cpp
  Renderer/Image.cpp
  Renderer/Rasterizer.cpp
  Renderer/RenderEngine.cpp
  Renderer/vk_mem_alloc.cpp
  Renderer/volk_impl.cpp

  Util/Filesystem.cpp
  Util/Logger.cpp
  Util/vk_util.cpp

  $<TARGET_OBJECTS:imgui>
)

add_library(${PROJECT_NAME}::hikari ALIAS hikari)

target_link_libraries(
  hikari
  PRIVATE
  project_options
  project_warnings
  volk_headers
  imgui
)

# Vulkan SDK version: 1.4.304.1
# find_package(Vulkan REQUIRED)

target_link_system_libraries(
  hikari
  PRIVATE
  # Vulkan::Vulkan
  glfw
  glm::glm
  vk-bootstrap::vk-bootstrap
  GPUOpen::VulkanMemoryAllocator

  # ktx
  # tinygltf

  PUBLIC
  spdlog::spdlog

  # CLI11::CLI11
)

target_include_directories(
  hikari
  ${WARNING_GUARD}
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>

  PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>

  # for export header
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
)

target_compile_features(hikari PUBLIC cxx_std_20)

set_target_properties(
  hikari
  PROPERTIES VERSION ${PROJECT_VERSION}
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

generate_export_header(hikari
BASE_NAME hkr
EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/hikari/hikari_export.hpp)

# choose WSI for linux box
set(WSI_SELECT XCB CACHE STRING "Choose the WSI.")
set_property(
  CACHE WSI_SELECT
  PROPERTY STRINGS
  "XCB" "XLIB" "WAYLAND"
)
if(WIN32 OR APPLE)
  mark_as_advanced(WSI_SELECT)
endif()

if(WIN32)
  target_compile_definitions(hikari PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
  target_compile_definitions(hikari PRIVATE VK_USE_PLATFORM_METAL_EXT)
elseif(UNIX)
  if(WSI_SELECT STREQUAL XCB)
    target_compile_definitions(hikari PRIVATE VK_USE_PLATFORM_XCB_KHR)
  elseif(WSI_SELECT STREQUAL XLIB)
    target_compile_definitions(hikari PRIVATE VK_USE_PLATFORM_XLIB_KHR)
  elseif(WSI_SELECT STREQUAL WAYLAND)
    target_compile_definitions(hikari PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)
  endif()
endif()
target_compile_definitions(
  hikari
  PRIVATE
  GLFW_INCLUDE_NONE
  GLFW_INCLUDE_VULKAN
  VK_NO_PROTOTYPES
  VMA_STATIC_VULKAN_FUNCTIONS=0
  VMA_DYNAMIC_VULKAN_FUNCTIONS=0
  $<$<CONFIG:Debug>:HKR_DEBUG>
  $<$<CONFIG:Debug>:ENABLE_VALIDATION_LAYER>
  $<$<CONFIG:Release>:HKR_RELEASE>
  $<$<CONFIG:RelWithDebInfo>:HKR_RELEASE>
)
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(hikari PUBLIC HIKARI_STATIC_DEFINE)
endif()

# compile shaders
find_program(
  GLSL_VALIDATOR glslangValidator
  HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/
)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "shaders/*.frag"
    "shaders/*.vert"
    "shaders/*.comp"
    "shaders/*.rgen"
    "shaders/*.rmiss"
    "shaders/*.rchit"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/assets/spirv/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  message(STATUS COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
  Shaders ALL
  DEPENDS ${SPIRV_BINARY_FILES}
)

