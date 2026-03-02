# Not active by default. Seed for future ArtifactCore CMake migration.
# Usage example (later): include(cmake/ArtifactCoreMigrationSeed.cmake)

cmake_minimum_required(VERSION 3.28)

if(TARGET ArtifactCore)
  return()
endif()

add_library(ArtifactCore STATIC)

target_compile_features(ArtifactCore PUBLIC cxx_std_23)

# Keep parity with current Debug|x64 vcxproj baseline where possible.
target_compile_definitions(ArtifactCore PRIVATE
  $<$<CONFIG:Debug>:_DEBUG>
  PLATFORM_WIN32
  _GUARDOVERFLOW_CRT_ALLOCATORS=1
  GLOG_NO_ABBREVIATED_SEVERITIES
  GLOG_USE_GLOG_EXPORT
  _CRT_SECURE_NO_WARNINGS
  _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
  LIBRARY_DLL_MODE=0
)

if(MSVC)
  target_compile_options(ArtifactCore PRIVATE /Zc:__cplusplus)
endif()

# TODO: Replace with explicit source list during real migration.
file(GLOB_RECURSE ARTIFACTCORE_SRC CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../src/*.cppm"
)
file(GLOB_RECURSE ARTIFACTCORE_INC CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/../include/*.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/*.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/../include/*.ixx"
)

target_sources(ArtifactCore PRIVATE ${ARTIFACTCORE_SRC} ${ARTIFACTCORE_INC})

target_include_directories(ArtifactCore
  PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/../include"
)

# Link list from current vcxproj (Debug|x64). Resolve with find_package/toolchain later.
target_link_libraries(ArtifactCore PRIVATE
  Opencv_world4110d
  libvlc
  libvlccore
  webp
  webpdecoder
  cpuinfo
  Sndfile
  SDL2-static
  box2d
  vulkan-1
  avformat
  avcodec
  avutil
  swscale
  ws2_32
)
