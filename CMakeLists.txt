cmake_minimum_required(VERSION 3.21)

# This template attempts to be "fetch_content"-able
# so that it works well with tools like CPM or other
# manual dependency management

# Only set the cxx_standard if it is not set by someone else
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()

# strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++20 and -std=gnu++20 for example
# when compiling with PCH enabled
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the project name and language
project(
  hikari
  VERSION 0.0.1
  LANGUAGES CXX C)

include(cmake/PreventInSourceBuilds.cmake)
include(ProjectOptions.cmake)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

project_setup_options()

project_global_options()
include(Dependencies.cmake)
project_setup_dependencies()

project_local_options()

# don't know if this should be set globally from here or not...
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(GIT_SHA
    "Unknown"
    CACHE STRING "SHA this build was generated from")
string(
  SUBSTRING "${GIT_SHA}"
            0
            8
            GIT_SHORT_SHA)

target_compile_features(project_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(${PROJECT_NAME}::project_options ALIAS project_options)
add_library(${PROJECT_NAME}::project_warnings ALIAS project_warnings)

#add_library(${PROJECT_NAME}::project_options INTERFACE IMPORTED)
#add_library(${PROJECT_NAME}::project_warnings INTERFACE IMPORTED)

# configure files based on CMake configuration options
# add_subdirectory(configured_files)

add_subdirectory(third_party)

# Adding the src:
add_subdirectory(src)

add_subdirectory(samples)
