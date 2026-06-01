# Motion Trail (Ghosting) Effect Implementation Memo

## Overview
The Motion Trail (Ghosting) effect blends the current video frame with historical frames to produce a temporal "afterimage" or trail.

## Design Decisions
1. **Stateful Effect**: This effect holds a history buffer (`ImageF32x4_RGBA`).
2. **Buffer Lifecycle**:
   - If the input image dimensions change (width/height), the history buffer is resized/re-allocated automatically.
   - If no history exists, the history is initialized with the current frame's content.
3. **Blend Modes**:
   - `Blend`: Classic linear interpolation. `output = current * (1 - decay) + history * decay`.
   - `Additive`: Accumulates brightness. Good for glowing trails.
   - `Maximum`: Takes the maximum component-wise. Good for highlights.

## API Layout
### Module Interface: `ImageProcessing` (Partition: `:MotionTrail`)
```cpp
// Within module ImageProcessing definition
export import :MotionTrail;
```
Or import directly via:
```cpp
import ImageProcessing;
```

## Detailed Blend Formulas (CPU / SIMD friendly loops)
*   **Decay logic**: Historical pixels are multiplied by `decay` first:
    `val_history_decayed = val_history * decay`
*   **Normal / Blend**:
    `val_out = val_current * (1.0f - intensity) + std::lerp(val_current, val_history_decayed, intensity)`
*   **Additive**:
    `val_out = clamp(val_current + val_history_decayed * intensity, 0.0f, 1.0f)`
*   **Maximum**:
    `val_out = max(val_current, val_history_decayed * intensity)`

## Exporting & Module discovery
The module has been successfully integrated and exported via `AbstractImageEffect.ixx` as a partition `:MotionTrail`. It can be imported in other parts of the application simply by importing `ImageProcessing`:
```cpp
import ImageProcessing;
```
