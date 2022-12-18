cmake_minimum_required(VERSION 3.8)

project(
  saf
  VERSION 0.1.0
  LANGUAGES CXX)

include(GNUInstallDirs)

add_library(saf INTERFACE)
target_include_directories(saf INTERFACE include/)

option(BUILD_EXAMPLES "" ON)
option(BUILD_TESTS "" ON)

if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(BUILD_TESTS)
    add_subdirectory(test)
endif()

# Install headers
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp")
