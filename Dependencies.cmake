include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(project_setup_dependencies)

  # For each dependency, see if it's
  # already been provided to us by a parent project

  if(NOT TARGET fmtlib::fmtlib)
    cpmaddpackage("gh:fmtlib/fmt#11.1.4")
  endif()

  if(NOT TARGET spdlog::spdlog)
    cpmaddpackage(
      NAME
      spdlog
      VERSION
      1.15.2
      GITHUB_REPOSITORY
      "gabime/spdlog"
      OPTIONS
      "SPDLOG_FMT_EXTERNAL ON"
    )
  endif()

  if(NOT TARGET glfw)
    cpmaddpackage(
      NAME GLFW
      GITHUB_REPOSITORY glfw/glfw
      GIT_TAG e7ea71be039836da3a98cea55ae5569cb5eb885c
      OPTIONS
      "GLFW_BUILD_TESTS OFF"
      "GLFW_BUILD_EXAMPLES OFF"
      "GLFW_BUILD_DOCS OFF"
    )
  endif()

  if(NOT TARGET glm::glm)
    cpmaddpackage(
      NAME glm
      GIT_TAG 2d4c4b4dd31fde06cfffad7915c2b3006402322f
      GITHUB_REPOSITORY g-truc/glm
    )
  endif()

endfunction()
