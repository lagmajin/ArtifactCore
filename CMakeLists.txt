cmake_minimum_required(VERSION 3.28)
project(ArtifactCore)

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# ソース収集とモジュール分類
# ---------------------------------------------------------------------------

file(GLOB_RECURSE POTENTIAL_SOURCES CONFIGURE_DEPENDS 
    "src/*.cpp" "src/*.cppm" "src/*.ixx"
    "include/*.ixx" "include/*.cppm"
    "NetworkRPCServer.ixx"
)

set(CORE_MODULES "")
set(CORE_IMPL "")
set(_PROVIDED_MODULES "")

# パーティション実装（module M:P;）は CXX_MODULES に含める必要がある
# 以下のファイルは Ninja が明示的に提供モジュールとしてスキャンしたもの
set(PARTITION_FILES
    "src/Image/OpenCV/ImageTransformCV.ixx"
    "src/ImageProcessing/NegativeCV.ixx"
    "src/ImageProcessing/NoiseImageGenerator.cppm"
    "src/ImageProcessing/OpenCV/AffineCV.cppm"
    "src/ImageProcessing/OpenCV/AntiAliasing.ixx"
    "src/ImageProcessing/OpenCV/HalfTone.ixx"
    "src/ImageProcessing/OpenCV/Monochrome.ixx"
    "src/ImageProcessing/OpenCV/Posterize.ixx"
    "src/ImageProcessing/OpenCV/VignetCV.ixx"
)

foreach(file_path ${POTENTIAL_SOURCES})
    # 除外チェック
    if(file_path MATCHES "stashed_broken_modules" OR 
       file_path MATCHES "disabled_modules" OR
       file_path MATCHES "/Halide/" OR
       file_path MATCHES "hostfxr" OR
       file_path MATCHES "\\.bak$" OR
       file_path MATCHES "DiligentEngine/DiligentCore/ThirdParty/Vulkan-Headers")
        continue()
    endif()

    set(is_interface_or_partition FALSE)
    
    # 1. パーティションリストにあるかチェック
    foreach(p_file ${PARTITION_FILES})
        if(file_path MATCHES "${p_file}$")
            set(is_interface_or_partition TRUE)
            break()
        endif()
    endforeach()

    # 2. ファイル内容で判定（export module ... または module ...:...）
    if(NOT is_interface_or_partition)
        file(READ "${file_path}" raw_content LIMIT 2048)
        string(REGEX REPLACE "//[^\n]*" "" content "${raw_content}")
        string(REGEX REPLACE "/\\*.*\\*/" "" content "${content}")

        if(content MATCHES "export[ \t\r\n]+module[ \t\r\n]+([a-zA-Z0-9\\._:]+)[ \t\r\n]*;" OR
           content MATCHES "module[ \t\r\n]+([a-zA-Z0-9\\._]+:[a-zA-Z0-9\\._:]+)[ \t\r\n]*;")
            set(is_interface_or_partition TRUE)
        endif()
    endif()

    if(is_interface_or_partition)
        list(APPEND CORE_MODULES "${file_path}")
    else()
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
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
)

# ---------------------------------------------------------------------------
# 外部ライブラリ探索
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
target_link_libraries(ArtifactCore PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)

find_package(OpenCV REQUIRED)
target_link_libraries(ArtifactCore PUBLIC ${OpenCV_LIBS})

find_package(SDL2 REQUIRED)
target_link_libraries(ArtifactCore PUBLIC SDL2::SDL2)

find_package(Boost REQUIRED COMPONENTS system iostreams)
target_link_libraries(ArtifactCore PUBLIC Boost::boost Boost::system Boost::iostreams)

find_package(Vulkan REQUIRED)
target_link_libraries(ArtifactCore PUBLIC Vulkan::Vulkan)

find_package(TBB REQUIRED)
target_link_libraries(ArtifactCore PUBLIC TBB::tbb)

find_package(OpenImageIO REQUIRED)
target_link_libraries(ArtifactCore PUBLIC OpenImageIO::OpenImageIO)

find_package(glm CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC glm::glm)

find_package(Stb REQUIRED)
target_include_directories(ArtifactCore PUBLIC ${Stb_INCLUDE_DIR})

find_package(assimp REQUIRED)
target_link_libraries(ArtifactCore PUBLIC assimp::assimp)

find_package(EnTT CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC EnTT::EnTT)

find_package(magic_enum CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC magic_enum::magic_enum)

if(MSVC)
    target_compile_definitions(ArtifactCore PRIVATE 
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()
