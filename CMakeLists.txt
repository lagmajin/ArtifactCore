cmake_minimum_required(VERSION 3.28)
project(ArtifactCore)

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# ソース収集とモジュール分類
# ---------------------------------------------------------------------------

# 全てのソース候補を収集
file(GLOB_RECURSE POTENTIAL_SOURCES CONFIGURE_DEPENDS 
    "src/*.cpp" "src/*.cppm" "src/*.ixx"
    "include/*.ixx" "include/*.cppm"
    "NetworkRPCServer.ixx"
)

set(CORE_MODULES "")
set(CORE_IMPL "")

# モジュール重複チェック用の追跡変数
# _PROVIDED_MODULE_<MODNAME> にパスを保存する

foreach(file_path ${POTENTIAL_SOURCES})
    # 除外ディレクトリ・ファイルのチェック
    if(file_path MATCHES "stashed_broken_modules" OR 
       file_path MATCHES "disabled_modules" OR
       file_path MATCHES "/Halide/" OR
       file_path MATCHES "hostfxr" OR
       file_path MATCHES "\\.bak$" OR
       file_path MATCHES "DiligentEngine/DiligentCore/ThirdParty/Vulkan-Headers") # vcpkgのvulkanと衝突するため除外
        continue()
    endif()

    # ファイルの内容を確認してモジュールの種類を判定
    file(READ "${file_path}" content LIMIT 1024)
    
    # 1. モジュールインターフェース (export module M; または export module M:P;)
    if(content MATCHES "export[ \t\r\n]+module[ \t\r\n]+([a-zA-Z0-9\\._:]+)[ \t\r\n]*;")
        set(mod_name "${CMAKE_MATCH_1}")
        
        if(DEFINED "_PROVIDED_MODULE_${mod_name}")
            message(STATUS "Skipping duplicate interface for module '${mod_name}': ${file_path} (Already provided by ${_PROVIDED_MODULE_${mod_name}})")
            list(APPEND CORE_IMPL "${file_path}") # 重複は通常ソース扱い（コンパイルエラーになる可能性が高いが、CMakeの停止は防げる）
        else()
            set("_PROVIDED_MODULE_${mod_name}" "${file_path}")
            list(APPEND CORE_MODULES "${file_path}")
        endif()

    # 2. モジュールパーティション実装 (module M:P;)
    # ※パーティション実装も CXX_MODULES ファイルセットに含める必要がある (CMake 3.28+ / Ninja)
    elseif(content MATCHES "[^export][ \t\r\n]*module[ \t\r\n]+([a-zA-Z0-9\\._]+:[a-zA-Z0-9\\._]+)[ \t\r\n]*;")
        set(mod_name "${CMAKE_MATCH_1}")
        
        if(DEFINED "_PROVIDED_MODULE_${mod_name}")
            message(STATUS "Skipping duplicate partition for module '${mod_name}': ${file_path}")
            list(APPEND CORE_IMPL "${file_path}")
        else()
            set("_PROVIDED_MODULE_${mod_name}" "${file_path}")
            list(APPEND CORE_MODULES "${file_path}")
        endif()

    # 3. 通常のモジュール実装 (module M;) または非モジュールファイル
    else()
        string(LENGTH "${content}" content_len)
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
