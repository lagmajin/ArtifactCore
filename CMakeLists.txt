cmake_minimum_required(VERSION 3.28)   # C++20 モジュール (FILE_SET CXX_MODULES) に 3.28 以上が必要
project(ArtifactCore)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# 外部依存パス（環境に合わせて -D オプションや cmake-presets で上書き可能）
# ---------------------------------------------------------------------------
set(VCPKG_INSTALLED   "X:/vcpkg/installed/x64-windows"          CACHE PATH "vcpkg install dir")
set(OPENCV_BUILD_DIR  "X:/opencv/build"                          CACHE PATH "OpenCV build dir")
set(VLC_DIR           "X:/vlc-3.0.21/sdk"                       CACHE PATH "VLC SDK dir")
set(BOX2D_DIR         "X:/box2d"                                 CACHE PATH "Box2D dir")
set(FMT_DIR           "X:/fmt/fmt"                               CACHE PATH "fmt dir")
set(SDL2_DIR          "X:/SDL2"                                  CACHE PATH "SDL2 dir")
set(LIBSND_DIR        "X:/libsnd"                                CACHE PATH "libsndfile dir")
set(THIRD_PARTY_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../third_party" CACHE PATH "local third_party dir")
set(ARTIFACT_APP_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../Artifact"  CACHE PATH "Artifact app dir (for shared includes)")

# ---------------------------------------------------------------------------
# ソース収集 (CONFIGURE_DEPENDS でファイル追加時に自動再構成)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CORE_IMPL CONFIGURE_DEPENDS
    "src/*.cpp"
)

# モジュールインターフェース / 実装ユニット
file(GLOB_RECURSE CORE_MODULES CONFIGURE_DEPENDS
    "include/*.ixx"
    "src/*.ixx"
    "src/*.cppm"
)

# Artifact アプリ側 include から ArtifactCore が直接参照している 2 ファイル
set(ARTIFACT_EXTRA_MODULES
    "${ARTIFACT_APP_DIR}/include/Audio/AudioSegment.ixx"
    "${ARTIFACT_APP_DIR}/include/Composition/CompositionSettings.ixx"
)
foreach(_f IN LISTS ARTIFACT_EXTRA_MODULES)
    if(EXISTS "${_f}")
        list(APPEND CORE_MODULES "${_f}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# ターゲット定義
# ---------------------------------------------------------------------------
add_library(ArtifactCore STATIC)

target_sources(ArtifactCore
    PRIVATE
        ${CORE_IMPL}
    PUBLIC
        FILE_SET CXX_MODULES FILES ${CORE_MODULES}
)

target_include_directories(ArtifactCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${ARTIFACT_APP_DIR}/include"
        "${THIRD_PARTY_DIR}"
)

# ---------------------------------------------------------------------------
# 外部ライブラリ探索
# ---------------------------------------------------------------------------

# Qt6
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets
    HINTS "${VCPKG_INSTALLED}/share/Qt6"
          "${VCPKG_INSTALLED}/share/Qt6Core"
)
if(Qt6_FOUND)
    target_link_libraries(ArtifactCore PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
    message(STATUS "ArtifactCore: Qt6 found")
else()
    # vcpkg ヘッダーだけでも通るよう include を追加
    target_include_directories(ArtifactCore PUBLIC
        "${VCPKG_INSTALLED}/include/Qt6"
        "${VCPKG_INSTALLED}/include/Qt6/QtCore"
        "${VCPKG_INSTALLED}/include/Qt6/QtGui"
    )
    message(WARNING "ArtifactCore: Qt6 not found via find_package, adding include dirs only")
endif()

# OpenCV
find_package(OpenCV QUIET HINTS "${OPENCV_BUILD_DIR}")
if(OpenCV_FOUND)
    target_include_directories(ArtifactCore PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(ArtifactCore PUBLIC ${OpenCV_LIBS})
    message(STATUS "ArtifactCore: OpenCV found")
else()
    target_include_directories(ArtifactCore PUBLIC "${OPENCV_BUILD_DIR}/include")
    message(WARNING "ArtifactCore: OpenCV not found via find_package, adding include dirs only")
endif()

# TBB
find_package(TBB QUIET HINTS "${VCPKG_INSTALLED}/share/tbb")
if(TBB_FOUND)
    target_link_libraries(ArtifactCore PUBLIC TBB::tbb)
    message(STATUS "ArtifactCore: TBB found")
endif()

# fmt
find_package(fmt QUIET HINTS "${FMT_DIR}/build" "${VCPKG_INSTALLED}/share/fmt")
if(fmt_FOUND)
    target_link_libraries(ArtifactCore PUBLIC fmt::fmt)
else()
    target_include_directories(ArtifactCore PUBLIC "${FMT_DIR}/include")
endif()

# SDL2
find_package(SDL2 QUIET HINTS "${SDL2_DIR}" "${VCPKG_INSTALLED}/share/sdl2")
if(SDL2_FOUND)
    target_link_libraries(ArtifactCore PUBLIC SDL2::SDL2)
else()
    target_include_directories(ArtifactCore PUBLIC "${SDL2_DIR}/include")
endif()

# Box2D
target_include_directories(ArtifactCore PUBLIC "${BOX2D_DIR}/include")

# libsndfile
target_include_directories(ArtifactCore PUBLIC "${LIBSND_DIR}/include")

# VLC
target_include_directories(ArtifactCore PUBLIC "${VLC_DIR}/include")

# dotnet hostfxr (ホスト側のヘッダー)
target_include_directories(ArtifactCore PUBLIC
    "C:/Program Files/dotnet/packs/Microsoft.NETCore.App.Host.win-x64/9.0.6/runtimes/win-x64/native"
)
