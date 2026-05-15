module;
#include <utility>

#include <cmath>
#include <algorithm>
#include <functional>
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing.Distortion;

import FloatRGBA;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore
{

 /// Distortion coordinate mapper.
 /// Maps destination (x,y) to source (sx,sy) sampling position.
 using DisplacementFunc = std::function<void(float srcX, float srcY,
                                              float imgW, float imgH,
                                              float& outSX, float& outSY)>;

 /// Bilinear-sampled pixel fetch.
 LIBRARY_DLL_API FloatRGBA sampleBilinear(const ImageF32x4_RGBA& src,
                                           float sx, float sy);

 /// Nearest-neighbor pixel fetch.
 LIBRARY_DLL_API FloatRGBA sampleNearest(const ImageF32x4_RGBA& src,
                                          float sx, float sy);

 /// Apply a displacement function to an image (CPU).
 /// For each output pixel, the displacement function maps to a source coordinate.
 LIBRARY_DLL_API void applyDisplacement(const ImageF32x4_RGBA& src,
                                         ImageF32x4_RGBA& dst,
                                         DisplacementFunc func,
                                         bool bilinear = true);

 // === Built-in displacement mappers ===

 /// Pinch (toward center) / Bulge (away from center).
 /// amount > 0 = pinch, amount < 0 = bulge.
 LIBRARY_DLL_API DisplacementFunc makePinchBulge(float centerX, float centerY,
                                                   float radius, float amount);

 /// Spherize (spherical lens distortion).
 /// amount in [-100, 100].
 LIBRARY_DLL_API DisplacementFunc makeSpherize(float centerX, float centerY,
                                                 float radius, float amount);

 /// Twirl (spiral rotation around center).
 /// angle in degrees.
 LIBRARY_DLL_API DisplacementFunc makeTwirl(float centerX, float centerY,
                                              float radius, float angle);

 /// Wave distortion (sinusoidal displacement).
 LIBRARY_DLL_API DisplacementFunc makeWave(float amplitudeX, float amplitudeY,
                                             float frequencyX, float frequencyY,
                                             float phaseX, float phaseY);

 /// Turbulent displacement (noise-based).
 /// Uses hash-based value noise internally.
 LIBRARY_DLL_API DisplacementFunc makeTurbulentDisplace(float amount,
                                                          float size,
                                                          float complexity,
                                                          int octaves,
                                                          float evolution);

 /// Optics compensation (barrel / pincushion lens distortion).
 /// fov in degrees, direction: +1 = undistort, -1 = distort.
 LIBRARY_DLL_API DisplacementFunc makeOpticsCompensation(float centerX, float centerY,
                                                           float fov, int direction);

 /// Bezier warp (4-corner mesh via bilinear interpolation).
 LIBRARY_DLL_API DisplacementFunc makeBilinearWarp(
     float tlX, float tlY,   // top-left
     float trX, float trY,   // top-right
     float blX, float blY,   // bottom-left
     float brX, float brY);  // bottom-right

 /// Offset (simple translation).
 LIBRARY_DLL_API DisplacementFunc makeOffset(float dx, float dy);

 /// Scale (zoom around center).
 LIBRARY_DLL_API DisplacementFunc makeScale(float centerX, float centerY,
                                              float scaleX, float scaleY);

 /// Rotate (around center). Angle in degrees.
 LIBRARY_DLL_API DisplacementFunc makeRotate(float centerX, float centerY,
                                               float angleDegrees);

 /// Mirror (flip around axis).
 LIBRARY_DLL_API DisplacementFunc makeMirror(float axisX, float axisY,
                                               bool horizontal, bool vertical);

 /// Noise-based displacement with explicit seed.
 LIBRARY_DLL_API DisplacementFunc makeNoiseDisplace(float amount, float size,
                                                      int seed, float evolution);

 // === Value noise (exposed for custom use) ===

 /// 2D value noise [0, 1].
 LIBRARY_DLL_API float valueNoise2D(float x, float y, int seed = 0);

 /// Fractal Brownian Motion (fBm) noise.
 LIBRARY_DLL_API float fbmNoise2D(float x, float y, int octaves, float lacunarity,
                                     float gain, int seed = 0);

};
