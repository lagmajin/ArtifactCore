# ArtifactCore Milestones Backlog

`ArtifactCore` 単体で空き時間に進めやすいよう、基盤ごとに小さめのマイルストーンへ分割したバックログ。

## Color / Image Processing

### C-IMG-1 Color Grading Foundation
- `ColorWheelsProcessor`
- `ColorCurves`
- `ColorGrader`
- 単体 API とサンプル適用経路の整理

### C-IMG-2 OpenCV Image Ops
- `ImageTransformCV`
- `ChromaKey`
- OpenCV ベースの変換処理の整理と再利用導線

### C-IMG-3 LUT / Color Utility Cleanup
- `ColorLUT`
- `ColorConversion`
- `AutoColorMatch`
- `ColorHarmonizer`
- 進捗: built-in LUT 名の列挙を追加し、UI/診断からの発見性を上げた
- 進捗: built-in LUT を LUTManager に一括登録できるようにした

## Video

### C-VID-1 Stabilizer Hardening
- `VideoStabilizer`
- `LiveStabilizer`
- `BatchStabilizer`
- API と結果確認導線の整理

### C-VID-2 Frame / Time Consistency
- `FramePosition`
- `FrameRange`
- `RationalTime`
- frame/time API の使い分け整理

### C-VID-3 Preview Quality Controls
- preview quality
- motion blur
- quality preset の整理

## Audio

### C-AUD-1 Mixer / Panner Core
- `AudioMixer`
- `AudioPanner`
- channel strip と基本ルーティング整理
- 進捗: mixer に bus 名一覧 / bus 検索 API を追加し、状態把握しやすくした
- 進捗: panning mode 名を表示できるようにした

### C-AUD-2 Device Layer Cleanup
- legacy `WASAPIDevice` residue cleanup
- `WASAPI(shared)` backend cleanup / `AudioRenderer` fallback selection
- `PortAudioDevice`
- device abstraction と fallback 動作整理

### C-AUD-3 Analysis / Waveform
- waveform
- level meter
- clock / segment / analysis API の整理

## Graphics / Creative Effects

### C-GFX-1 Creative Effect Base
- `CreativeEffect`
- `CreativeEffectManager`
- parameter 表現と effect stack 基盤の統一
- CPU reference / HLSL backend split
- CPU/HLSL diff test harness

### C-GFX-2 Creative Effect Pack
- `Halftone`
- `Posterize`
- `Pixelate`
- `Mirror`
- `Kaleidoscope`
- `Fisheye`
- `Glitch`
- `OldTV`

### C-GFX-3 Shader / Compute Utility Cleanup
- shader helper
- blend shader
- blur shader
- texture utility の整理

### C-GFX-4 Object Fracture / Shatter Effect
- shard generation
- debris emission
- collision / lifetime / dissolve
- CPU reference / GPU backend split

## Layer / Composition Core

### C-TXT-1 Text Style Foundation
- `TextStyle`
- `ParagraphStyle`
- alignment / tracking / leading の共通化
- font family resolution / fallback

### C-TXT-2 Text Layer Integration
- `ArtifactTextLayer` への style 適用
- property / serialization の受け皿

### C-LYR-1 Layer Serialization Cleanup
- layer JSON 保存
- type name / class name
- compatibility path の整理

### C-LYR-2 Composition / Layer Ownership
- composition-layer 関係
- insertion / move / duplication API
- dirty propagation の整理

### C-LYR-3 Solid / Media / Text Layer Normalization
- `Solid`
- `Media`
- `Text`
- `Null`
- 各レイヤーの共通責務整理

### C-LYR-4 Source Abstraction Foundation
- `ISource`
- `FileSource`
- `GeneratedSource`
- source metadata / capability / relink 基盤
- layer 種別より source 中心へ寄せる準備

## Render / Playback

### C-RND-1 Software Render Bridge
- software composition 向けの共通 effect 適用層
- layer から render backend への橋渡し

### C-RND-2 Render Graph / Cache Cleanup
- frame cache
- scheduler
- render path の責務分離

### C-RND-3 Playback Service Consistency
- playback context
- audio/video sync
- frame stepping の整理

## Architecture / Toolchain

### C-ARC-1 import std Rollout
- module 単位で `import std;` へ段階移行
- old include 塊の削減

### C-ARC-2 Module Naming Cleanup
- module 名とファイル名のズレ解消
- deprecated module の棚卸し

### C-ARC-3 Diagnostics / Test Utilities
- core 側の検証 API
-小さな test helper
- サンプルパイプラインの整備

### C-ARC-4 Internal Event System Foundation
- `EventBus`
- typed subscribe / publish / post
- UI thread bridge の前提になる queued delivery
- Qt 非依存の高速イベント基盤

## Good Small Tasks

- `C-ARC-1 import std Rollout`
- `C-LYR-3 Solid / Media / Text Layer Normalization`
- `C-AUD-3 Analysis / Waveform`
- `C-GFX-2 Creative Effect Pack`
- `C-IMG-2 OpenCV Image Ops`
