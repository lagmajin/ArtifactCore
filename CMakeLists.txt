cmake_minimum_required(VERSION 3.30)
project(ArtifactCore)

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "")

# ---------------------------------------------------------------------------
# ソース収集とモジュール分類
# ---------------------------------------------------------------------------

# 全てのソース候補を収集
file(GLOB_RECURSE POTENTIAL_SOURCES CONFIGURE_DEPENDS 
    "src/*.cpp" "src/*.cppm" "src/*.ixx"
    "src/**/*.cpp" "src/**/*.cppm" "src/**/*.ixx"
    "include/*.ixx" "include/*.cppm"
    "include/**/*.ixx" "include/**/*.cppm"
    "NetworkRPCServer.ixx"
)

set(CORE_MODULES "")
set(CORE_IMPL "")
set(_PROVIDED_MODULES "")

foreach(file_path ${POTENTIAL_SOURCES})
    # 除外チェック
    if(file_path MATCHES "stashed_broken_modules" OR
       file_path MATCHES "disabled_modules" OR
       file_path MATCHES "/Halide/" OR
       file_path MATCHES "hostfxr" OR
       file_path MATCHES "\\.bak$" OR
       file_path MATCHES "src/Graphics/Effect/HalftoneEffect\\.cppm" OR
       file_path MATCHES "src/Graphics/Effect/MirrorEffect\\.cppm" OR
       file_path MATCHES "src/Graphics/Effect/KaleidoscopeEffect\\.cppm" OR
       file_path MATCHES "src/Graphics/Effect/CreativeEffectFactory\\.cppm" OR
       file_path MATCHES "src/Graphics/Effect/OldTVEffect\\.cppm" OR
       file_path MATCHES "src/ImageProcessing/OpenCV/VignetCV\\.ixx" OR
       file_path MATCHES "src/ImageProcessing/OpenCV/HalfTone\\.ixx" OR
       file_path MATCHES "src/ImageProcessing/OpenCV/AntiAliasing\\.ixx" OR
       file_path MATCHES "include/Asset/AsseetImporter\\.ixx" OR
       file_path MATCHES "src/Image/FFmpegEncoder\\.Test\\.cppm" OR
       file_path MATCHES "DiligentEngine/DiligentCore/ThirdParty/Vulkan-Headers")
        continue()
    endif()

    # ファイルの内容を確認してモジュールの種類を判定
    # 以前の 4096 バイト制限では、大量の include の後に export module がある場合に
    # 検出に失敗することがあったため、制限を 32KB に拡大します。
    file(READ "${file_path}" raw_content LIMIT 32768)
    
    # コメントを除去して誤判定を防ぐ
    # ※ CMake の REGEX REPLACE は最短一致ではないため、注意が必要ですが、
    #    ここでは大まかな判定用なので許容します。
    string(REGEX REPLACE "//[^\n]*" "" content "${raw_content}")
    # 改行を含むコメントも大まかに除去
    string(REGEX REPLACE "/\\*.*\\*/" "" content "${content}")

    set(is_mod_interface_or_partition FALSE)
    set(found_mod_name "")

    # 1. インターフェース判定: export module M;
    # より柔軟な正規表現を使用します（セミコロンの前の空白を許容）
    if(content MATCHES "export[ \t\r\n]+module[ \t\r\n]+([a-zA-Z0-9\\._:]+)[ \t\r\n]*;")
        set(is_mod_interface_or_partition TRUE)
        set(found_mod_name "${CMAKE_MATCH_1}")
    # 2. パーティション判定: module M:P; (コロンを含む)
    elseif(content MATCHES "module[ \t\r\n]+([a-zA-Z0-9\\._]+:[a-zA-Z0-9\\._:]+)[ \t\r\n]*;")
        set(is_mod_interface_or_partition TRUE)
        set(found_mod_name "${CMAKE_MATCH_1}")
    endif()

    if(is_mod_interface_or_partition)
        # 重複チェック (同一モジュール名を提供するファイルが複数あると Ninja が止まる)
        list(FIND _PROVIDED_MODULES "${found_mod_name}" _mod_idx)
        if(_mod_idx EQUAL -1)
            list(APPEND CORE_MODULES "${file_path}")
            list(APPEND _PROVIDED_MODULES "${found_mod_name}")
            # message(STATUS "Module detected: ${found_mod_name} (${file_path})")
        else()
            # 重複モジュールを通常ソースに落とすと
            # 「provides module but not in FILE_SET CXX_MODULES」を誘発するため除外する
            message(WARNING "Duplicate module name detected: ${found_mod_name} in ${file_path} (skipped)")
        endif()
    else()
        # 通常のソースファイル (module M; または非モジュール)
        string(LENGTH "${raw_content}" content_len)
        if(content_len GREATER 0)
            list(APPEND CORE_IMPL "${file_path}")
        endif()
    endif()
endforeach()

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
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
target_link_libraries(ArtifactCore PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)

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
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        message(STATUS "CUDA Toolkit found: enabling ggml CUDA backend alongside Vulkan")
        if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
            # Keep the default CUDA build narrow to reduce compile time and binary size.
            set(CMAKE_CUDA_ARCHITECTURES "86-real" CACHE STRING "CUDA architectures for ggml-cuda" FORCE)
            if(CUDAToolkit_VERSION VERSION_GREATER_EQUAL "11.8")
                set(CMAKE_CUDA_ARCHITECTURES "86-real;89-real" CACHE STRING "CUDA architectures for ggml-cuda" FORCE)
            endif()
        endif()
        set(GGML_CUDA ON CACHE BOOL "" FORCE)
    else()
        message(STATUS "CUDA Toolkit not found: keeping ggml Vulkan backend only")
        set(GGML_CUDA OFF CACHE BOOL "" FORCE)
    endif()
    set(GGML_METAL OFF CACHE BOOL "" FORCE)
    set(GGML_WEBGPU OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    set(GGML_CPU_ALL_VARIANTS ON CACHE BOOL "" FORCE)
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
    _artifact_align_llama_output(ggml-vulkan)
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

# FFmpeg
find_package(ffmpeg REQUIRED)
if(ffmpeg_FOUND)
    target_include_directories(ArtifactCore PUBLIC ${FFMPEG_INCLUDE_DIR})
    target_link_libraries(ArtifactCore PUBLIC 
        ${FFMPEG_AVCODEC_LIBRARY}
        ${FFMPEG_AVFORMAT_LIBRARY}
        ${FFMPEG_AVUTIL_LIBRARY}
        ${FFMPEG_SWSCALE_LIBRARY}
        ${FFMPEG_SWRESAMPLE_LIBRARY}
    )
else()
    # Fallback for manual specification
    target_link_libraries(ArtifactCore PUBLIC
        "${VCPKG_INSTALLED}/lib/avcodec.lib"
        "${VCPKG_INSTALLED}/lib/avformat.lib"
        "${VCPKG_INSTALLED}/lib/avutil.lib"
        "${VCPKG_INSTALLED}/lib/swscale.lib"
        "${VCPKG_INSTALLED}/lib/swresample.lib"
    )
    target_include_directories(ArtifactCore PUBLIC "${VCPKG_INSTALLED}/include")
endif()

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
