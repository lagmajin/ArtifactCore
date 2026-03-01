module;

#include "../Define/DllExportMacro.hpp"

export module Math.Noise;

import std;

export namespace ArtifactCore {

class LIBRARY_DLL_API NoiseGenerator {
public:
    // 1D Perlin Noise
    static float perlin(float x);

    // 2D Perlin Noise
    static float perlin(float x, float y);

    // 3D Perlin Noise
    static float perlin(float x, float y, float z);

    // Fractal Noise / Fractional Brownian Motion (fBm)
    // octaves: 重ね合わせる層の数, persistence: 各層の振幅の減衰率, lacunarity: 各層の周波数の倍率
    static float fractal(float x, float y, float z, int octaves = 4, float persistence = 0.5f, float lacunarity = 2.0f);

    // Worley Noise / Voronoi Noise (F1, F2 などを返せるように拡張可能)
    static float worley(float x, float y, float z);

    // シード値の設定
    static void setSeed(unsigned int seed);

private:
    static float fade(float t);
    static float lerp(float t, float a, float b);
    static float grad(int hash, float x, float y, float z);
};

} // namespace ArtifactCore
