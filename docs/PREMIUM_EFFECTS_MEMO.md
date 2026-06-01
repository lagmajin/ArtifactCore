# Premium Effects Pack Implementation Memo

This memo documents the mathematical formulations, stateful lifecycle rules, and file maps implemented for the four premium effects: Tilt-Shift, Anamorphic Flare, Duotone, and EdgeEcho.

---

## 1. Tilt-Shift (ティルトシフト)
Provides miniaturization focus bands.

### Mathematical Formulation
To make CPU blurring extremely lightweight, we avoid per-pixel dynamic-radius box/Gaussian blurs. Instead:
1. Generate a single highly blurred version of the source image $I_{\text{blur}}$ using a highly optimized horizontal/vertical box-blur pass.
2. For each pixel $(x, y)$, calculate normalized distance from the horizontal focus line $Y_{\text{focus}}$:
   - $D = \left| \frac{y}{\text{height}} - Y_{\text{focus}} \right|$
3. Compute the blend factor $W$:
   - $W = \text{clamp}\left( \frac{D - \text{focusWidth} \cdot 0.5f}{\text{blurFalloff}}, 0.0f, 1.0f \right) \cdot \text{maxBlur}$
4. Linear-interpolate between original pixel $I_{\text{src}}$ and blurred pixel $I_{\text{blur}}$:
   - $I_{\text{out}} = I_{\text{src}} \cdot (1.0f - W) + I_{\text{blur}} \cdot W$

### File Map
- Interface Partition: [TiltShift.ixx](file:///x:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/TiltShift.ixx)
- Implementation Module: [TiltShift.cppm](file:///x:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/TiltShift.cppm)

---

## 2. Anamorphic Flare (アナモルフィック・フレア)
Generates horizontal anamorphic cinema streaks from light sources.

### Mathematical Formulation
1. **Luminance Extraction**: Extract highlights exceeding `threshold`:
   - $L = 0.299 \cdot R + 0.587 \cdot G + 0.114 \cdot B$
   - If $L > \text{threshold}$, keep pixel colors; otherwise, discard to $(0,0,0,0)$.
2. **Horizontal Streaks**: For each pixel, accumulate horizontal light leakage with exponential falloff (`flareLength` determines decay rate):
   - $C_{\text{flare}}(x, y) = \sum_{dx = -W}^{W} C_{\text{src}}(x + dx, y) \cdot \text{decay}^{|dx|}$
   - Crucially optimized to run in $O(N)$ time complexity via a dual horizontal sweep (left-to-right and right-to-left decay sweep).
3. **Tinting & Addition**: Multiply flare by `tint` and `intensity`, then blend additively onto the original pixel:
   - $I_{\text{out}} = \text{clamp}(I_{\text{src}} + C_{\text{flare}} \cdot \text{tint} \cdot \text{intensity}, 0.0f, 1.0f)$

### File Map
- Interface Partition: [AnamorphicFlare.ixx](file:///x:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/AnamorphicFlare.ixx)
- Implementation Module: [AnamorphicFlare.cppm](file:///x:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/AnamorphicFlare.cppm)

---

## 3. Duotone (デュオトーン)
Applies a two-color mapped gradient to image pixels based on their perceived brightness.

### Mathematical Formulation
1. Get perceptual luminance of pixel:
   - $L = 0.299 \cdot R + 0.587 \cdot G + 0.114 \cdot B$
2. Linearly interpolate between `shadowColor` ($C_{\text{shadow}}$) and `highlightColor` ($C_{\text{highlight}}$) based on $L$:
   - $C_{\text{mapped}} = \text{lerp}(C_{\text{shadow}}, C_{\text{highlight}}, L)$
3. Blend the mapped color back with the original pixel based on `blend`:
   - $I_{\text{out}} = \text{lerp}(I_{\text{src}}, C_{\text{mapped}}, \text{blend})$

### File Map
- Interface Partition: [Duotone.ixx](file:///x:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Duotone.ixx)
- Implementation Module: [Duotone.cppm](file:///x:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/Duotone.cppm)

---

## 4. EdgeEcho (エッジエコー)
Creates glowing, temporal echoes of outlines that delay and wave.

### Mathematical Formulation
1. **Edge Detection**: Sobel operator to compute horizontal and vertical gradients:
   - $G_x = \text{SobelX}(I_{\text{src}})$, $G_y = \text{SobelY}(I_{\text{src}})$
   - Magnitude: $M = \sqrt{G_x^2 + G_y^2}$
   - Edge thresholding: If $M > \text{edgeThreshold}$, keep edge; otherwise, $0$.
2. **Warped History**: Apply wave distortion (sinusoidal offset to vertical coordinate based on time) to the outline history buffer:
   - $y' = y + \text{waveAmp} \cdot \sin(2\pi \cdot \text{waveFreq} \cdot x + t)$
3. **Decay & Blend**: Decay history outline by `decay`, add current edge, and overlay `echoColor` onto the original canvas.

### File Map
- Interface Partition: [EdgeEcho.ixx](file:///x:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/EdgeEcho.ixx)
- Implementation Module: [EdgeEcho.cppm](file:///x:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/EdgeEcho.cppm)

---

## Exporting & Module discovery
All four partitions have been successfully integrated and exported via `AbstractImageEffect.ixx`. They are automatically visible by importing the core `ImageProcessing` module:
```cpp
import ImageProcessing;
```
