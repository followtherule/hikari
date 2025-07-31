# volk
add_subdirectory(volk)

# imgui
file(GLOB imgui_src
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_vulkan.cpp"
)
set(imgui_src ${imgui_src} PARENT_SCOPE)
set(imgui_include
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui
  PARENT_SCOPE
)
set(imgui_backends_include
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
  PARENT_SCOPE
)
