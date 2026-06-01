# Creative Effects Implementation Memo (Kaleidoscope & Halftone)

## 1. Kaleidoscope (万華鏡) Effect
The Kaleidoscope effect is implemented by extending the existing CPU spatial coordinate displacement mapping pipeline.

### Mathematical Formulation
Given a target output coordinate $(x, y)$ and center $(cx, cy)$:
1. Translate relative to center: $dx = x - cx$, $dy = y - cy$
2. Convert to polar coordinates:
   - Radius $r = \sqrt{dx^2 + dy^2}$
   - Angle $\theta = \text{atan2}(dy, dx)$
3. Apply rotation and mirror symmetry based on segments $N$ and angle offset $A$:
   - Rotate: $\theta' = \theta - A$
   - Sector angle width: $\phi = 2\pi / N$
   - Local sector angle: $\theta_{\text{local}} = \text{fmod}(\theta', \phi)$
   - Handle negative fmod: if $\theta_{\text{local}} < 0$, $\theta_{\text{local}} += \phi$
   - Mirror reflection: if $\theta_{\text{local}} > \phi / 2$, $\theta_{\text{local}} = \phi - \theta_{\text{local}}$
4. Convert back to Cartesian source coordinates:
   - $sx = cx + r \cdot \cos(\theta_{\text{local}} + A)$
   - $sy = cy + r \cdot \sin(\theta_{\text{local}} + A)$
5. Fetch pixel using bilinear interpolation from $(sx, sy)$.

### Implementation Details
- Declared in: [Distortion.ixx](file:///x:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Distortion.ixx)
- Implemented in: [Distortion.cppm](file:///x:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/Distortion.cppm)
- Integrated into `makeKaleidoscope(...)` and available as a standard `DisplacementFunc`.

---

## 2. Halftone (ハーフトーン) Effect
The Halftone effect transforms image luminance values into a spatial grid of proportional dots.

### Parameter Definitions
- `dotSize` (float): The spacing/width of the grid cells.
- `angle` (float): The rotation angle of the grid structure (degrees).
- `contrast` (float): Enhances the difference between bright and dark dot thresholds.
- `isColor` (bool): 
  - `false` (Monochrome): Black dots on white background.
  - `true` (Color): Scale the original pixel colors by the dot factor.

### Mathematical Formulation
For each pixel $(x, y)$:
1. Rotate coordinates by grid angle:
   - $xr = x \cdot \cos(angle) - y \cdot \sin(angle)$
   - $yr = x \cdot \sin(angle) + y \cdot \cos(angle)$
2. Map to local grid cell:
   - Cell index: $cx = \lfloor xr / dotSize \rfloor$, $cy = \lfloor yr / dotSize \rfloor$
   - Center of cell in rotated space: $c_x' = (cx + 0.5) \cdot dotSize$, $c_y' = (cy + 0.5) \cdot dotSize$
   - Distance to cell center in rotated space: $d = \sqrt{(xr - c_x')^2 + (yr - c_y')^2}$
3. Map cell center back to original space to sample luminance:
   - $orig\_cx = c_x' \cdot \cos(-angle) - c_y' \cdot \sin(-angle)$
   - $orig\_cy = c_x' \cdot \sin(-angle) + c_y' \cdot \cos(-angle)$
4. Get local luminance $L$ at $(orig\_cx, orig\_cy)$ using clamped sampling to prevent out-of-bound errors.
5. Calculate dot radius:
   - $R_{dot} = (dotSize / 2) \cdot (1.0f - \text{clamp}(L \cdot contrast, 0.0f, 1.0f))$
6. Draw factor:
   - If $d < R_{dot}$, pixel is "dot" (factor = 0.0f).
   - Else, pixel is "background" (factor = 1.0f).
7. Apply color mode:
   - Monochrome: output `(factor, factor, factor, 1.0f)`.
   - Color: output `(original * factor)`.

### Implementation Details
- Interface Partition: [Halftone.ixx](file:///x:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Halftone.ixx)
- Implementation Module: [Halftone.cppm](file:///x:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/Halftone.cppm)
- Exported via [AbstractImageEffect.ixx](file:///x:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/AbstractImageEffect.ixx) under partition `:Halftone`. It is automatically visible by importing the core `ImageProcessing` module.
