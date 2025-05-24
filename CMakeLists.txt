cmake_minimum_required(VERSION 3.8...3.20)

project(
  saf
  VERSION 0.1.0
  LANGUAGES CXX)

set(SAF_IS_ROOT OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(SAF_IS_ROOT ON)
endif()

option(SAF_BUILD_TESTS "Build SAF tests" ${SAF_IS_ROOT})
option(SAF_BUILD_EXAMPLES "Build SAF examples" ${SAF_IS_ROOT})

add_library(saf INTERFACE)
target_include_directories(saf INTERFACE include/)

if(SAF_BUILD_EXAMPLES)
  add_subdirectory(example)
endif()

if(SAF_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# Install headers
include(GNUInstallDirs)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp")
