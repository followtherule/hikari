cmake_minimum_required(VERSION 3.15...3.23)

set(APP_NAME hikari_app)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")

add_executable(${APP_NAME} sample.cpp)
target_link_libraries(
  ${APP_NAME}
  PRIVATE
  hikari::project_warnings
  hikari::project_options
  hikari::hikari
)

target_include_directories(
  ${APP_NAME}
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

configure_file("settings.h.in" "${CMAKE_BINARY_DIR}/settings.h" ESCAPE_QUOTES)
