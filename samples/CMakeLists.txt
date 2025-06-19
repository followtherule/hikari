cmake_minimum_required(VERSION 3.15...3.23)

set(APP_NAME sample)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}")

add_executable(${APP_NAME} sample.cpp)
target_link_libraries(
  ${APP_NAME}
  PRIVATE
  hikari::project_warnings
  hikari::project_options
  hikari::hikari
)

