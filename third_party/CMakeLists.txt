# volk
add_subdirectory(volk)

# imgui
file(GLOB imgui_src
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_vulkan.cpp"
)
add_library(imgui OBJECT ${imgui_src})
target_include_directories(
  imgui
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/volk
)
target_compile_definitions(imgui PUBLIC
  IMGUI_IMPL_VULKAN_USE_VOLK
)
