include(GenerateExportHeader)

add_library(hikari)

target_sources(
  hikari
  PRIVATE
  Core/App.cpp
  Core/Window.cpp

  Renderer/Buffer.cpp
  Renderer/Cube.cpp
  Renderer/Descriptor.cpp
  Renderer/Image.cpp
  Renderer/Model.cpp
  Renderer/Pipeline.cpp
  Renderer/Rasterizer.cpp
  Renderer/Raytracer.cpp
  Renderer/RenderEngine.cpp
  Renderer/Skybox.cpp
  Renderer/tiny_gltf_impl.cpp
  Renderer/vk_mem_alloc.cpp
  Renderer/volk_impl.cpp

  Util/Filesystem.cpp
  Util/Logger.cpp
  Util/vk_util.cpp

  ${imgui_src}
)

target_sources(
  hikari
  PRIVATE
  FILE_SET private_headers TYPE HEADERS
  BASE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  FILE_SET imgui_include TYPE HEADERS
  BASE_DIRS
  ${imgui_include}
  FILE_SET imgui_backends_include TYPE HEADERS
  BASE_DIRS
  ${imgui_backends_include}
)

generate_export_header(
  hikari
  BASE_NAME hkr
  EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/hikari/hikari_export.hpp
)

target_sources(
  hikari
  PUBLIC
  FILE_SET public_headers TYPE HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include
  FILES
  ${PROJECT_SOURCE_DIR}/include/hikari/Core/App.h
  ${PROJECT_SOURCE_DIR}/include/hikari/Core/EntryPoint.h

  ${PROJECT_SOURCE_DIR}/include/hikari/Util/Logger.h

  ${PROJECT_BINARY_DIR}/include/hikari/hikari_export.hpp
)

add_library(${PROJECT_NAME}::hikari ALIAS hikari)

target_link_libraries(
  hikari
  PRIVATE
  project_options
  project_warnings
  volk_headers
)

target_link_system_libraries(
  hikari
  PRIVATE
  glfw
  glm::glm
  vk-bootstrap::vk-bootstrap
  GPUOpen::VulkanMemoryAllocator
  ktx
  tinygltf

  PUBLIC
  spdlog::spdlog

  # CLI11::CLI11
)

target_compile_features(hikari PUBLIC cxx_std_20)

set_target_properties(
  hikari
  PROPERTIES VERSION ${PROJECT_VERSION}
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

# choose WSI for linux box
set(WSI_SELECT XCB CACHE STRING "Choose the WSI: XCB XLIB WAYLAND.")
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
  IMGUI_IMPL_VULKAN_USE_VOLK
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

find_program(
  GLSLC glslc
  HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/bin/ $ENV{VULKAN_SDK}/Bin32/
)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "shaders/*.frag"
    "shaders/*.vert"
    "shaders/*.comp"
    "shaders/*.rgen"
    "shaders/*.rmiss"
    "shaders/*.rchit"
    "shaders/*.rahit"
)

set(GLSL_ARGS --target-spv=spv1.5)
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/assets/spirv/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  message(STATUS COMMAND ${GLSLC} ${GLSL} -o ${SPIRV} ${GLSL_ARGS})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSLC} ${GLSL} -o ${SPIRV} ${GLSL_ARGS}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
  Shaders ALL
  DEPENDS ${SPIRV_BINARY_FILES}
)

