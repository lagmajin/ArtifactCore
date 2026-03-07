cmake_minimum_required(VERSION 3.28)
project(ArtifactCore)

# CMakeキャッシュをリセット（モジュール除外後のビルドのため）
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# 外部依存パス (強制的に現在の環境に合わせる)
# ---------------------------------------------------------------------------
# Removed explicit vcpkg paths, using manifest mode instead.

# ---------------------------------------------------------------------------
# ソース収集
# ---------------------------------------------------------------------------
# 実装ファイル (.cpp および src 内の .cppm/.ixx)
file(GLOB_RECURSE CORE_IMPL CONFIGURE_DEPENDS 
    "src/*.cpp" 
    "src/*.cppm" 
    "src/*.ixx"
)

# モジュールインターフェース (.ixx)
file(GLOB_RECURSE CORE_MODULES CONFIGURE_DEPENDS 
    "include/*.ixx" 
    "include/*.cppm"
)

# src/ にあるが「モジュールの一部（インターフェース/パーティション）」として機能しているファイルを CORE_MODULES に追加
set(SRC_INTERFACES
    "src/Audio/AudioVolume.ixx"
    "src/Audio/AudioWriter.cppm"
    "src/ColorCollection/ColorGrading.cppm"
    "src/ImageProcessing/Halide/Defines.ixx"
    "src/ImageProcessing/ImageF32x1.ixx"
    "src/Knob/Knob.ixx"
    "src/Time/TimeRemap.cppm"
    "src/Video/Stabilizer.cppm"
    "src/Animation/TransformModule.ixx"
    "src/Image/OpenCV/ImageTransformCV.ixx"
    "src/ImageProcessing/NegativeCV.ixx"
    "src/ImageProcessing/NoiseImageGenerator.cppm"
    "src/ImageProcessing/OpenCV/AffineCV.cppm"
    "src/ImageProcessing/OpenCV/AntiAliasing.ixx"
    "src/ImageProcessing/OpenCV/HalfTone.ixx"
    "src/ImageProcessing/OpenCV/Monochrome.ixx"
    "src/ImageProcessing/OpenCV/Posterize.ixx"
    "src/ImageProcessing/OpenCV/VignetCV.ixx"
    "NetworkRPCServer.ixx"
)

foreach(iface ${SRC_INTERFACES})
    set(full_path "${CMAKE_CURRENT_SOURCE_DIR}/${iface}")
    if(EXISTS ${full_path})
        list(APPEND CORE_MODULES ${full_path})
        list(REMOVE_ITEM CORE_IMPL ${full_path})
    endif()
endforeach()

# 使っていない Halide 関連ファイルを除外
file(GLOB_RECURSE HALIDE_FILES "src/ImageProcessing/Halide/*.cpp" "src/ImageProcessing/Halide/*.cppm")
if(HALIDE_FILES)
    list(REMOVE_ITEM CORE_IMPL ${HALIDE_FILES})
endif()

# VLC 関連ファイルは stashed_broken_modules に移動済み

# 使っていない hostfxr 関連ファイルを除外 (coreclr_delegates.h不足のため)
file(GLOB_RECURSE HOSTFXR_FILES "src/Script/hostfxr.cpp")
if(HOSTFXR_FILES)
    list(REMOVE_ITEM CORE_IMPL ${HOSTFXR_FILES})
endif()

# OpenCV G-API を使っている不要なファイルも除外（必要なら戻せます）
file(GLOB_RECURSE GAPI_FILES 
    "src/ImageProcessing/OpenCV/FilmGrain.cpp"
    "src/ImageProcessing/Filter.cpp"
)
if(GAPI_FILES)
    list(REMOVE_ITEM CORE_IMPL ${GAPI_FILES})
endif()

# 使っていない Halide 関連のモジュールを除外
file(GLOB_RECURSE HALIDE_MODULES 
    "include/*/Halide/*.ixx"
    "src/*/Halide/*.ixx"
    "src/*/Halide/*.cppm"
)
if(HALIDE_MODULES)
    list(REMOVE_ITEM CORE_MODULES ${HALIDE_MODULES})
endif()

# その他、依存ライブラリ不足や未整備のモジュールを除外
file(GLOB_RECURSE EXTRA_EXCLUDES
    "include/Script/hostfxr.ixx"
    "include/Script/hostfxr.ixx.bak"
    "src/Script/hostfxr.cppm"
    "src/Script/hostfxr.cppm.bak"
    "include/Layer/LayerMatte.ixx"
    "include/ImageProcessing/OpenCV/G-API/BlurGAPI.ixx"
    # 循環依存を修正するまで除外（ファイルをリネームしたため）
    "include/Asset/ProxyCacheManager.ixx.bak"
    "include/Asset/ProxyGenerator.ixx.bak"
    "include/Asset/ProxySettings.ixx.bak"
    "include/Audio/AudioDucking.ixx.bak"
    "include/Audio/WaveformGenerator.ixx.bak"
    "include/Effect/Transition.ixx.bak"
)
if(EXTRA_EXCLUDES)
    list(REMOVE_ITEM CORE_MODULES ${EXTRA_EXCLUDES})
endif()

# ---------------------------------------------------------------------------
# ターゲット定義
# ---------------------------------------------------------------------------
add_library(ArtifactCore STATIC)

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
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    PRIVATE
        # Diligent の親ディレクトリ（libs/DiligentEngine）を追加
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
)

# ---------------------------------------------------------------------------
# 外部ライブラリ探索 (Qt6, OpenCV, SDL2, Boost, Vulkan, etc.)
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

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(ArtifactCore PUBLIC Vulkan::Vulkan)

# TBB
find_package(TBB REQUIRED)
target_link_libraries(ArtifactCore PUBLIC TBB::tbb)

# OpenImageIO
find_package(OpenImageIO REQUIRED)
target_link_libraries(ArtifactCore PUBLIC OpenImageIO::OpenImageIO)

# GLM
find_package(glm CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC glm::glm)

# stb
find_package(Stb REQUIRED)
target_include_directories(ArtifactCore PUBLIC ${Stb_INCLUDE_DIR})

# assimp
find_package(assimp REQUIRED)
target_link_libraries(ArtifactCore PUBLIC assimp::assimp)

# EnTT
find_package(EnTT CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC EnTT::EnTT)

if(MSVC)
    target_compile_definitions(ArtifactCore PRIVATE 
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()
