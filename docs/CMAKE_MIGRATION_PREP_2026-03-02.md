# ArtifactCore CMake Migration Prep (2026-03-02)

## Goal
- Keep using `.sln` / `.vcxproj` for day-to-day development.
- Prepare a low-risk path to migrate `ArtifactCore` to CMake later.
- Do not change current build behavior now.

## What Was Prepared Today
1. Captured current MSBuild baseline from `ArtifactCore.vcxproj`.
2. Created an offline CMake seed file (`cmake/ArtifactCoreMigrationSeed.cmake`) that is not wired into existing builds.
3. Documented phased migration checklist and risk controls in this file.

## Current Baseline (from ArtifactCore.vcxproj)
- Project file: `ArtifactCore/ArtifactCore.vcxproj`
- Language standard (Debug|x64): `stdcpplatest`
- Extra compiler option: `/Zc:__cplusplus`
- Key preprocessor definitions (Debug|x64):
  - `_DEBUG`
  - `_CONSOLE`
  - `PLATFORM_WIN32`
  - `_GUARDOVERFLOW_CRT_ALLOCATORS=1`
  - `GLOG_NO_ABBREVIATED_SEVERITIES`
  - `GLOG_USE_GLOG_EXPORT`
  - `_CRT_SECURE_NO_WARNINGS`
  - `_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING`
  - `LIBRARY_DLL_MODE=0`
- IncludePath snapshot contains (examples):
  - `X:\vcpkg\installed\x64-windows\include\Qt6...`
  - `X:\Build\install\include`
  - `X:\opencv\build\include`
  - `X:\vlc-3.0.21\sdk\include`
  - `X:\SDL2\include`, `X:\libsnd\include`, `X:\box2d\include`
  - `C:\Users\lagma\Desktop\Artifact\third_party`
- LibraryPath snapshot contains (examples):
  - `X:\Build\install\lib\DiligentCore\Debug`
  - `X:\Build\Graphics\GraphicsEngineVulkan\Debug`
  - `X:\opencv\build\x64\vc16\lib`
  - `X:\vlc-3.0.21\sdk\lib`
  - `$(SolutionDir)\lib`
- AdditionalDependencies (Debug|x64):
  - `Opencv_world4110d.lib`
  - `libvlc.lib`, `libvlccore.lib`
  - `webp.lib`, `webpdecoder.lib`
  - `cpuinfo.lib`
  - `Sndfile.lib`
  - `SDL2-static.lib`
  - `box2d.lib`
  - `vulkan-1.lib`
  - `avformat.lib`, `avcodec.lib`, `avutil.lib`, `swscale.lib`
  - `ws2_32.lib`

## Source Layout Snapshot
- `ArtifactCore/include` + `ArtifactCore/src` extensions count:
  - `.ixx`: 381
  - `.cpp`: 149
  - `.cppm`: 84
  - `.axt`: 27
  - `.hlsl`: 17
  - `.hpp`: 15
  - `.h`: 6

## Files Added For Prep
- `ArtifactCore/cmake/ArtifactCoreMigrationSeed.cmake`

This seed file is intentionally not connected to current `.sln/.vcxproj` flow.

## Migration Plan (when you decide to start)
1. Freeze baseline
- Confirm current Debug|x64 build is green in Visual Studio.
- Export one build log and keep as reference.

2. CMake side-by-side bootstrap
- Generate CMake target for `ArtifactCore` only.
- Keep output artifact name same as today if possible.

3. Dependency normalization
- Replace absolute paths with:
  - `find_package(...)`
  - toolchain file (e.g. vcpkg)
  - cache variables for local SDK roots

4. Module/source onboarding
- Add a curated subset first (small vertical slice).
- Then expand by area (`Core`, `Image`, `Media`, etc.).

5. Validation gates
- ABI check by linking existing app against CMake-built core.
- Runtime smoke tests for media decode/render paths.

6. Switch-over
- Promote CMake target to default only after parity is verified.

## Risks and Controls
- Risk: absolute local paths in vcxproj do not port.
  - Control: route all external paths through CMake cache/toolchain variables.
- Risk: C++ modules behavior differs across generators/toolsets.
  - Control: start with same MSVC toolset and keep standards/options aligned.
- Risk: large source surface causes slow migration.
  - Control: migrate by component and keep both build systems in parallel temporarily.

## Notes
- No existing `.sln/.vcxproj` setting was changed by this prep.
- This is documentation and seed-only groundwork.
