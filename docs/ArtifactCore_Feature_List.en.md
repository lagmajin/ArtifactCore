# ArtifactCore Feature List

Last updated: 2026-03-02

This document provides a high-level feature map of the ArtifactCore library.
It is intended as an index for developers to quickly locate major systems.

## Core Architecture

- C++20 modules-based core library
- Domain-oriented modules under `include/`
- Runtime implementations under `src/`
- UI-independent logic prioritized for reuse

## Feature Areas

### Timeline and Playback

- `Playback/PlaybackClock`: high-precision playback clock, speed and loop control
- `Frame/*`: frame position, range, rate, offset, time
- `Time/*`: timecode, duration, rational time, remap helpers
- `Media/MediaPlaybackController`: media-oriented playback control

### Input and Keymap System (Blender-like foundation)

- `UI/InputOperator`: context-aware input processing
- `UI/KeyMap`: configurable keymap definitions and bindings
- `UI/InputEvent`: normalized input event model
- `ActionManager` and `Action`: action registration and execution hub

### Composition and Layer Foundations

- `Composition/*`: composition buffers and pre-compose support
- `Layer/*`: layer state, strip, blend/matte related data
- `Track/LayerTrack`: layer track abstractions

### Image and Graphics

- `Image/*`: image containers and format bridges
- `ImageProcessing/*`: OpenCV, DirectCompute, Halide pipelines
- `Graphics/*`: GPU texture, shader, PSO/cache, rendering helpers
- `Render/*`: render queue, settings, render worker metadata

### Video and Codec

- `Codec/*`: MF/FFmpeg/GStreamer decoder/encoder utilities
- `Video/*`: decoder/encoder interfaces and playback helpers
- `Media/*`: media source, probe, frame/audio decoder wrappers

### Audio

- `Audio/*`: mixer, renderer, panner, ducking, ring buffer, waveform
- audio format/frame/provider abstractions

### Effects and Processing

- `Effect/*`: transition and effect base utilities
- `Color/*` and `ColorCollection/*`: color types, LUT, grading helpers
- `Mask/RotoMask`: rotoscoping and mask data model
- `Tracking/MotionTracker`: motion tracking primitives

### Geometry, Transform, and Shape

- `Geometry/*`: mesh import/export and math helpers
- `Mesh/*`: mesh data and Wavefront support
- `Transform/*`: 2D/3D transform components
- `Shape/*`: vector path/layer/group definitions

### Script and Expression

- `Script/*`: expression parser/evaluator and script engine pieces
- builtin function and environment management modules

### Assets, IO, and Platform

- `Asset/*`: asset metadata, proxy generation/cache
- `IO/*`: async file/image import-export utilities
- `File/*` and `FileSystem/*`: file info/type and directory support
- `Platform/*`: OS/platform helpers and process/shell utilities

### Utility and Infrastructure

- `Utils/*`: IDs, strings, path, timers, explorer helpers
- `Container/*`: multi-index container utilities
- `Event/EventBus`: event dispatch infrastructure
- `Thread/*`: threading helpers
- `EnvironmentVariable/*`: environment query/update support

## Existing Focused Docs in This Repository

- `docs/TimelineClock_Architecture.md`
- `docs/PlaybackClock_Usage.md`
- `docs/MFFrameExtractor_Usage.md`
- `docs/FrameRange_Usage.md`

## Notes

- This file is a discovery index, not a full API reference.
- For concrete usage, start from the focused docs above and the corresponding `include/*` module.
