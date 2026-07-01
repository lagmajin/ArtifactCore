cmake_minimum_required(VERSION 3.30)
project(ArtifactCore)

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "")

# ---------------------------------------------------------------------------
# ソース収集とモジュール分類
# ---------------------------------------------------------------------------

# 拡張子ベースの分類:
#   .ixx → 常にモジュールインターフェース（内容スキャン不要）
#   .cpp → 常に実装ファイル（内容スキャン不要）
#   .cppm → export module の有無をスキャンして振り分け
file(GLOB_RECURSE IXX_FILES
    "include/*.ixx" "include/**/*.ixx"
    "src/*.ixx" "src/**/*.ixx"
    "NetworkRPCServer.ixx"
    "NetworkRPCClient.ixx"
)
file(GLOB_RECURSE CPP_FILES
    "src/*.cpp" "src/**/*.cpp"
)
file(GLOB_RECURSE CPPM_FILES
    "include/*.cppm" "include/**/*.cppm"
    "src/*.cppm" "src/**/*.cppm"
)

set(CORE_MODULES "")
set(CORE_IMPL "")
set(_PROVIDED_MODULES "")

# アーカイブ除外フィルター
function(_artifact_exclude_check file_path result_var)
    if("${file_path}" MATCHES "stashed_broken_modules" OR
       "${file_path}" MATCHES "disabled_modules" OR
       "${file_path}" MATCHES "[/\\\\]Halide[/\\\\]" OR
       "${file_path}" MATCHES "hostfxr" OR
       "${file_path}" MATCHES "\\.bak$" OR
       "${file_path}" MATCHES "src/ImageProcessing/OpenCV/VignetCV\\.ixx" OR
       "${file_path}" MATCHES "src/ImageProcessing/OpenCV/HalfTone\\.ixx" OR
       "${file_path}" MATCHES "src/ImageProcessing/OpenCV/AntiAliasing\\.ixx" OR
       "${file_path}" MATCHES "src/Diagnostics/ValidationRules\\.cppm" OR
       "${file_path}" MATCHES "src/Animation/EasingCurveUtil\\.cppm" OR
       "${file_path}" MATCHES "src/Graphics/Compute/CurveComputer\\.cppm" OR
       "${file_path}" MATCHES "src/Graphics/Compute/DuotoneComputer\\.cppm" OR
       "${file_path}" MATCHES "src/Graphics/Compute/EchoBlendComputer\\.cppm" OR
       "${file_path}" MATCHES "src/Graphics/Compute/EdgeEchoComputer\\.cppm" OR
       "${file_path}" MATCHES "src/Graphics/Compute/HalftoneComputer\\.cppm" OR
       "${file_path}" MATCHES "src/Graphics/Compute/Histogram\\.cppm" OR
       "${file_path}" MATCHES "src/Graphics/Compute/ScopeComputer\\.cppm" OR
       "${file_path}" MATCHES "SpinTransition\\.cppm$" OR
       "${file_path}" MATCHES "src/ImageProcessing/AnamorphicFlare\\.cppm" OR
       "${file_path}" MATCHES "include/Diagnostics/ValidationRules\\.ixx" OR
       "${file_path}" MATCHES "include[/\\\\]Asset[/\\\\]AsseetImporter\\.ixx" OR
       "${file_path}" MATCHES "src/Image/FFmpegEncoder\\.Test\\.cppm" OR
       "${file_path}" MATCHES "src/Render/SoftwareRayTracer\\.Test\\.cppm" OR
       "${file_path}" MATCHES "DiligentEngine/DiligentCore/ThirdParty/Vulkan-Headers")
        set(${result_var} TRUE PARENT_SCOPE)
    else()
        set(${result_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

# .ixx → モジュールインターフェース（内容スキャン不要）
foreach(file_path ${IXX_FILES})
    _artifact_exclude_check("${file_path}" _excluded)
    if(_excluded)
        continue()
    endif()
    list(APPEND CORE_MODULES "${file_path}")
endforeach()

# .cpp → 実装ファイル（内容スキャン不要）
foreach(file_path ${CPP_FILES})
    _artifact_exclude_check("${file_path}" _excluded)
    if(_excluded)
        continue()
    endif()
    list(APPEND CORE_IMPL "${file_path}")
endforeach()

# .cppm → export module スキャンで振り分け
foreach(file_path ${CPPM_FILES})
    _artifact_exclude_check("${file_path}" _excluded)
    if(_excluded)
        continue()
    endif()

    file(READ "${file_path}" raw_content LIMIT 32768)
    string(REGEX REPLACE "//[^\n]*" "" content "${raw_content}")
    string(REGEX REPLACE "/\\*.*\\*/" "" content "${content}")

    set(_mod_name "")
    if(content MATCHES "export[ \t\r\n]+module[ \t\r\n]+([a-zA-Z0-9\\._:]+)[ \t\r\n]*;")
        set(_mod_name "${CMAKE_MATCH_1}")
    elseif(content MATCHES "module[ \t\r\n]+([a-zA-Z0-9\\._]+:[a-zA-Z0-9\\._:]+)[ \t\r\n]*;")
        set(_mod_name "${CMAKE_MATCH_1}")
    endif()

    if(_mod_name STREQUAL "")
        list(APPEND CORE_IMPL "${file_path}")
    elseif(content MATCHES "export[ \t\r\n]+module[ \t\r\n]+[a-zA-Z0-9\\._:]+[ \t\r\n]*;")
        list(FIND _PROVIDED_MODULES "${_mod_name}" _mod_idx)
        if(_mod_idx EQUAL -1)
            list(APPEND CORE_MODULES "${file_path}")
            list(APPEND _PROVIDED_MODULES "${_mod_name}")
        else()
            message(WARNING "Duplicate module name detected: ${_mod_name} in ${file_path} (skipped)")
        endif()
    else()
        list(APPEND CORE_IMPL "${file_path}")
    endif()
endforeach()

# CMake's module scan can miss implementation partitions in this tree when the
# module unit is only provided as `module M:Impl;`, so register the known shape
# partitions explicitly to keep FILE_SET CXX_MODULES complete.
foreach(_artifact_force_module IN ITEMS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Geometry/Procedural3DGenerators.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/ShapeOperator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/TrimPaths.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/Repeater.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/ShapeGroup.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Memory/UniquePtr.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Memory/TrackedPtr.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Shape/ShapeGroup.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Shape/ShapePath.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Shape/ShapeLayer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Collaborate/CollaborationProtocol.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/TemplateSlot.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/TemplateLock.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/ParametricComposition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Simulation/PyroSimulation.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Image/FFmpegEncoder.Helpers.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ImageBuffer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Memory/SharedPtr.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Memory/ArtifactArray.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Core/ArtifactAtomic.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Core/ArtifactHashMap.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Core/ArtifactOptional.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Container/SmallVector.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Container/NameMap.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Container/IdMap.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Physics/SandSim2D.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Graphics/SandGPUCompute.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Core/ArtifactSpan.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Core/ArtifactFunction.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Core/ArtifactVariant.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Core/ArtifactUtility.ixx"
)
    list(FIND CORE_MODULES "${_artifact_force_module}" _artifact_force_module_index)
    if(_artifact_force_module_index EQUAL -1)
        list(APPEND CORE_MODULES "${_artifact_force_module}")
    endif()
endforeach()

# Procedural3DGenerators.cppm is the implementation unit for the explicitly
# registered public generator module above.
list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Geometry/Procedural3DGenerators.cppm" _procedural_3d_impl_idx)
if(_procedural_3d_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Geometry/Procedural3DGenerators.cppm")
endif()

# LUT3DComputer.cppm is the implementation unit for the interface declared in
# include/Graphics/Compute/LUT3DComputer.ixx. Keep it as a private source so it
# does not generate a second module interface unit.
list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Compute/LUT3DComputer.cppm" _lut3d_computer_impl_idx)
if(_lut3d_computer_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Compute/LUT3DComputer.cppm")
endif()

list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Simulation/PyroSimulation.cppm" _pyro_simulation_impl_idx)
if(_pyro_simulation_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Simulation/PyroSimulation.cppm")
endif()

list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Physics/SandSim2D.cppm" _sand_sim_impl_idx)
if(_sand_sim_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Physics/SandSim2D.cppm")
endif()

list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/SandGPUCompute.cppm" _sand_gpu_impl_idx)
if(_sand_gpu_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/SandGPUCompute.cppm")
endif()

# ---------------------------------------------------------------------------
# ターゲット定義
# ---------------------------------------------------------------------------
add_library(ArtifactCore STATIC)

set_target_properties(ArtifactCore PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
)

# MSVC specific C++23 modules settings
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCore PRIVATE /WX-)
    # Enable C++23 standard (latest)
    target_compile_options(ArtifactCore PRIVATE /std:c++latest)
    # Enable modules support
    target_compile_options(ArtifactCore PRIVATE /experimental:module)
    # Enable full C++23 library support
    target_compile_options(ArtifactCore PRIVATE /Zc:__cplusplus /permissive-)
endif()

target_sources(ArtifactCore
    PRIVATE ${CORE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${CORE_MODULES}
)

target_include_directories(ArtifactCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Core"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Image"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Grid"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Render"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Color"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Acoustic"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
)

if(MSVC)
    get_filename_component(_artifact_msvc_toolchain_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_artifact_msvc_toolchain_dir "${_artifact_msvc_toolchain_dir}" DIRECTORY)
    get_filename_component(_artifact_msvc_toolchain_dir "${_artifact_msvc_toolchain_dir}" DIRECTORY)
    get_filename_component(_artifact_msvc_toolchain_dir "${_artifact_msvc_toolchain_dir}" DIRECTORY)
    target_include_directories(ArtifactCore SYSTEM BEFORE PUBLIC "${_artifact_msvc_toolchain_dir}/include")

    set(_artifact_windows_sdk_root "C:/Program Files (x86)/Windows Kits/10")
    if(EXISTS "${_artifact_windows_sdk_root}/Include")
        file(GLOB _artifact_windows_sdk_versions LIST_DIRECTORIES true "${_artifact_windows_sdk_root}/Include/*")
        list(SORT _artifact_windows_sdk_versions COMPARE NATURAL ORDER DESCENDING)
        list(GET _artifact_windows_sdk_versions 0 _artifact_windows_sdk_version_dir)
        target_include_directories(ArtifactCore SYSTEM BEFORE PUBLIC
            "${_artifact_windows_sdk_version_dir}/ucrt"
            "${_artifact_windows_sdk_version_dir}/shared"
            "${_artifact_windows_sdk_version_dir}/um"
            "${_artifact_windows_sdk_version_dir}/winrt"
        )
    endif()
endif()

# DiligentEngine
set(DILIGENT_CORE_LIBS "")
set(DILIGENT_CORE_COMPILE_DEFINITIONS "")
if(TARGET Diligent-GraphicsEngineD3D12-static)
    list(APPEND DILIGENT_CORE_LIBS Diligent-GraphicsEngineD3D12-static)
elseif(TARGET Diligent-GraphicsEngineD3D12-shared)
    list(APPEND DILIGENT_CORE_LIBS Diligent-GraphicsEngineD3D12-shared)
    list(APPEND DILIGENT_CORE_COMPILE_DEFINITIONS DILIGENT_D3D12_SHARED=1)
endif()
if(TARGET Diligent-GraphicsEngineD3DBase)
    list(APPEND DILIGENT_CORE_LIBS Diligent-GraphicsEngineD3DBase)
endif()
if(TARGET Diligent-Common)
    list(APPEND DILIGENT_CORE_LIBS Diligent-Common)
endif()
if(DILIGENT_CORE_LIBS)
    target_link_libraries(ArtifactCore PUBLIC ${DILIGENT_CORE_LIBS})
endif()
if(DILIGENT_CORE_COMPILE_DEFINITIONS)
    list(REMOVE_DUPLICATES DILIGENT_CORE_COMPILE_DEFINITIONS)
    target_compile_definitions(ArtifactCore PUBLIC ${DILIGENT_CORE_COMPILE_DEFINITIONS})
endif()

# ---------------------------------------------------------------------------
# 外部ライブラリ探索
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
target_link_libraries(ArtifactCore PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

find_package(OpenCV REQUIRED)
target_link_libraries(ArtifactCore PUBLIC ${OpenCV_LIBS})

find_package(SDL2 REQUIRED)
target_link_libraries(ArtifactCore PUBLIC SDL2::SDL2)

# Boost (Asio, UUID, Iostreams)
find_package(Boost REQUIRED COMPONENTS system iostreams)
target_link_libraries(ArtifactCore PUBLIC Boost::boost Boost::system Boost::iostreams)

# Vulkan (headers only — volk handles dynamic loading; linking vulkan-1.lib
# would shadow volk's writable global symbols with read-only import thunks)
find_package(Vulkan REQUIRED)
target_link_libraries(ArtifactCore PUBLIC Vulkan::Headers)

find_package(OpenXR REQUIRED)
target_link_libraries(ArtifactCore PUBLIC OpenXR::headers)

# ggml-vulkan shader generation needs a real glslc executable.
# Some environments resolve Vulkan_GLSLC_EXECUTABLE to a vcpkg path that does
# not actually contain glslc.exe, so prefer a concrete SDK install if needed.
if(NOT EXISTS "${Vulkan_GLSLC_EXECUTABLE}")
    find_program(ARTIFACT_VULKAN_GLSLC_EXECUTABLE
        NAMES glslc
        HINTS
            ENV VulkanSDK
        PATHS
            "C:/VulkanSDK"
        PATH_SUFFIXES
            Bin
        REQUIRED
    )
    set(Vulkan_GLSLC_EXECUTABLE "${ARTIFACT_VULKAN_GLSLC_EXECUTABLE}" CACHE FILEPATH
        "Preferred glslc executable for llama.cpp Vulkan shader generation" FORCE)
endif()

# TBB
find_package(TBB REQUIRED)
target_link_libraries(ArtifactCore PUBLIC TBB::tbb)

# OpenImageIO
find_package(OpenImageIO REQUIRED)
target_link_libraries(ArtifactCore PUBLIC OpenImageIO::OpenImageIO)

find_package(glm CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC glm::glm)

find_package(Stb REQUIRED)
target_include_directories(ArtifactCore PUBLIC ${Stb_INCLUDE_DIR})

find_package(EnTT CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC EnTT::EnTT)

find_package(magic_enum CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC magic_enum::magic_enum)

find_package(protobuf CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)

# Locate sentencepiece using explicit paths derived from the include dir to avoid
# the PATH_SUFFIXES bug where a Debug-build CMAKE_PREFIX_PATH causes the release
# find_library to resolve to the debug/lib folder.
unset(SENTENCEPIECE_INCLUDE_DIR       CACHE)
unset(SENTENCEPIECE_LIBRARY_RELEASE   CACHE)
unset(SENTENCEPIECE_LIBRARY_DEBUG     CACHE)

find_path(SENTENCEPIECE_INCLUDE_DIR
    NAMES sentencepiece_processor.h
    REQUIRED
)
# Derive the vcpkg triplet prefix from the include directory (…/x64-windows/include → …/x64-windows)
get_filename_component(_sp_prefix "${SENTENCEPIECE_INCLUDE_DIR}" DIRECTORY)
find_library(SENTENCEPIECE_LIBRARY_RELEASE
    NAMES sentencepiece
    PATHS "${_sp_prefix}/lib"
    NO_DEFAULT_PATH
    REQUIRED
)
find_library(SENTENCEPIECE_LIBRARY_DEBUG
    NAMES sentencepiece
    PATHS "${_sp_prefix}/debug/lib"
    NO_DEFAULT_PATH
    REQUIRED
)

add_library(sentencepiece::sentencepiece STATIC IMPORTED GLOBAL)
set_target_properties(sentencepiece::sentencepiece PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SENTENCEPIECE_INCLUDE_DIR}"
    IMPORTED_LOCATION_RELEASE "${SENTENCEPIECE_LIBRARY_RELEASE}"
    IMPORTED_LOCATION_DEBUG   "${SENTENCEPIECE_LIBRARY_DEBUG}"
    INTERFACE_LINK_LIBRARIES  "protobuf::libprotobuf;absl::abseil_dll;absl::flags"
)
target_link_libraries(ArtifactCore PUBLIC sentencepiece::sentencepiece)

find_package(onnxruntime CONFIG REQUIRED)
if(TARGET onnxruntime::onnxruntime)
    target_link_libraries(ArtifactCore PUBLIC onnxruntime::onnxruntime)
else()
    target_link_libraries(ArtifactCore PUBLIC onnxruntime)
endif()

# llama.cpp
set(ARTIFACT_LLAMA_CPP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../libs/llama.cpp")
if(NOT TARGET llama AND EXISTS "${ARTIFACT_LLAMA_CPP_SOURCE_DIR}/CMakeLists.txt")
    set(_artifact_prev_scan_for_modules "${CMAKE_CXX_SCAN_FOR_MODULES}")
    set(_artifact_prev_build_shared_libs_defined FALSE)
    if(DEFINED BUILD_SHARED_LIBS)
        set(_artifact_prev_build_shared_libs "${BUILD_SHARED_LIBS}")
        set(_artifact_prev_build_shared_libs_defined TRUE)
    endif()
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

    set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_WEBUI OFF CACHE BOOL "" FORCE)
    set(LLAMA_OPENSSL OFF CACHE BOOL "" FORCE)
    set(LLAMA_LLGUIDANCE OFF CACHE BOOL "" FORCE)
    set(GGML_BACKEND_DL ON CACHE BOOL "" FORCE)
    set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
    # CUDA backend is intentionally disabled in the parent build to keep the
    # local verification loop stable on machines without a complete CUDA host toolchain.
    set(GGML_CUDA OFF CACHE BOOL "" FORCE)
    set(GGML_METAL OFF CACHE BOOL "" FORCE)
    set(GGML_WEBGPU OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    # Build a single CPU variant to keep compile time and binary size down.
    set(GGML_CPU_ALL_VARIANTS OFF CACHE BOOL "" FORCE)
    set(GGML_CCACHE OFF CACHE BOOL "" FORCE)

    add_subdirectory("${ARTIFACT_LLAMA_CPP_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/llama.cpp" EXCLUDE_FROM_ALL)

    if(_artifact_prev_build_shared_libs_defined)
        set(BUILD_SHARED_LIBS "${_artifact_prev_build_shared_libs}" CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()
    set(CMAKE_CXX_SCAN_FOR_MODULES "${_artifact_prev_scan_for_modules}")
endif()

if(TARGET llama)
    target_link_libraries(ArtifactCore PUBLIC llama)

    function(_artifact_align_llama_output target_name)
        if(TARGET ${target_name})
            set_target_properties(${target_name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
                ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
        endif()
    endfunction()

    _artifact_align_llama_output(llama)
    _artifact_align_llama_output(ggml)
    _artifact_align_llama_output(ggml-base)
    _artifact_align_llama_output(ggml-cpu)
    _artifact_align_llama_output(ggml-metal)
    _artifact_align_llama_output(ggml-cuda)
else()
    find_package(llama REQUIRED CONFIG)
    target_link_libraries(ArtifactCore PUBLIC llama)
endif()

# ufbx (vendored)
if(TARGET ufbx)
    target_link_libraries(ArtifactCore PUBLIC ufbx)
endif()

# FFmpeg - Direct linking with explicit library paths
# These paths are set by the vcpkg integration and passed to CMake
# The libraries exist in: out/vcpkg_installed/x64-windows/lib and debug/lib

# Get the vcpkg installed directory from the toolchain file
# Usually: out/vcpkg_installed
if(NOT DEFINED VCPKG_INSTALLED_DIR)
    # Try to infer from CMAKE_TOOLCHAIN_FILE
    get_filename_component(_vcpkg_root_guess "${CMAKE_CURRENT_LIST_DIR}/../out/vcpkg_installed" ABSOLUTE)
    if(EXISTS "${_vcpkg_root_guess}")
        set(VCPKG_INSTALLED_DIR "${_vcpkg_root_guess}")
    else()
        # Fallback: use VCPKG_ROOT
        set(VCPKG_INSTALLED_DIR "${VCPKG_ROOT}/installed" CACHE PATH "vcpkg installed directory")
    endif()
endif()

message(STATUS "Using vcpkg directory: ${VCPKG_INSTALLED_DIR}")

# Determine the vcpkg triplet (x64-windows for MSVC on x64)
set(_vcpkg_triplet "x64-windows")

# Set lib and include directories
set(_vcpkg_lib_dir "${VCPKG_INSTALLED_DIR}/${_vcpkg_triplet}/lib")
set(_vcpkg_debug_lib_dir "${VCPKG_INSTALLED_DIR}/${_vcpkg_triplet}/debug/lib")
set(_vcpkg_include_dir "${VCPKG_INSTALLED_DIR}/${_vcpkg_triplet}/include")

message(STATUS "FFmpeg library dir: ${_vcpkg_lib_dir}")
message(STATUS "FFmpeg debug lib dir: ${_vcpkg_debug_lib_dir}")

# Link the FFmpeg libraries directly by full path
# Use $<CONFIG:Debug> generator expression for config-specific linking
target_link_libraries(ArtifactCore PUBLIC
    $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/avcodec.lib>
    $<$<CONFIG:Release>:${_vcpkg_lib_dir}/avcodec.lib>
    $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/avformat.lib>
    $<$<CONFIG:Release>:${_vcpkg_lib_dir}/avformat.lib>
    $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/avutil.lib>
    $<$<CONFIG:Release>:${_vcpkg_lib_dir}/avutil.lib>
    $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/swscale.lib>
    $<$<CONFIG:Release>:${_vcpkg_lib_dir}/swscale.lib>
    $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/swresample.lib>
    $<$<CONFIG:Release>:${_vcpkg_lib_dir}/swresample.lib>
)

# Add FFmpeg include directory
target_include_directories(ArtifactCore PUBLIC ${_vcpkg_include_dir})

message(STATUS "FFmpeg libraries linked: avcodec, avformat, avutil, swscale, swresample")

if(ARTIFACT_FRONTEND_MSVC)
    target_link_libraries(ArtifactCore PUBLIC
        mfplat
        mfreadwrite
        mfuuid
        mf
        winmm
    )
    target_compile_definitions(ArtifactCore PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

# Python scripting support (pybind11 embedded interpreter)
find_package(Python3 QUIET COMPONENTS Interpreter Development)
if(Python3_FOUND)
    find_package(pybind11 QUIET)
    if(pybind11_FOUND)
        target_link_libraries(ArtifactCore PUBLIC pybind11::headers Python3::Python)
        target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_PYTHON=1)
        message(STATUS "Python scripting enabled: pybind11 + Python ${Python3_VERSION}")
    endif()
endif()

# AngelScript scripting support (Unity-MonoBehaviour-style layer components).
# Provided by vcpkg (angelscript[addons]). When the package is found, the
# ARTIFACT_HAS_ANGELSCRIPT macro enables the real implementation; otherwise
# AngelScriptEngine compiles as a no-op stub.
find_package(Angelscript CONFIG QUIET)
if(Angelscript_FOUND)
    target_link_libraries(ArtifactCore PUBLIC Angelscript::angelscript)
    target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_ANGELSCRIPT=1)
    message(STATUS "AngelScript scripting enabled: Angelscript ${Angelscript_VERSION}")
else()
    message(STATUS "AngelScript not found (AngelScriptEngine will be a stub)")
endif()
