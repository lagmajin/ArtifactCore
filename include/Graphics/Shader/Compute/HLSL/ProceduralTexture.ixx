module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.ProceduralTexture;

export namespace ArtifactCore::Shaders::ProceduralTexture
{
inline constexpr const char* ProceduralTextureShaderText = R"(
cbuffer ProceduralTextureCB : register(b0)
{
    uint4 g_Head0;
    uint4 g_Head1;
    float4 g_Primary0;
    float4 g_Primary1;
    float4 g_Post0;
    float4 g_Post1;
    float4 g_Primary2;
    float4 g_Secondary0;
    float4 g_Secondary1;
    uint4 g_Secondary2;
    uint4 g_Secondary3;
    float4 g_Warp0;
    float4 g_Warp1;
    float4 g_Warp2;
};

RWTexture2D<float4> OutTex : register(u0);

static const float PI = 3.14159265358979323846f;

uint HashU32(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint Hash2(uint2 p, uint seed)
{
    return HashU32(p.x ^ (p.y * 0x9e3779b9u) ^ (seed * 0x85ebca6bu));
}

float Hash01(uint h)
{
    return (h & 0x00ffffffu) / 16777215.0f;
}

float2 Rotate2(float2 v, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(c * v.x - s * v.y, s * v.x + c * v.y);
}

float2 GradientFromHash(uint h, float rotation)
{
    float a = Hash01(h) * 6.28318530718f + rotation;
    return float2(cos(a), sin(a));
}

float Fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float ValueNoise(float2 p, int2 period, uint seed)
{
    int2 i = int2(floor(p));
    float2 f = frac(p);

    int2 p00 = int2((i.x % period.x + period.x) % period.x,
                    (i.y % period.y + period.y) % period.y);
    int2 p10 = int2(((i.x + 1) % period.x + period.x) % period.x,
                    (i.y % period.y + period.y) % period.y);
    int2 p01 = int2((i.x % period.x + period.x) % period.x,
                    ((i.y + 1) % period.y + period.y) % period.y);
    int2 p11 = int2(((i.x + 1) % period.x + period.x) % period.x,
                    ((i.y + 1) % period.y + period.y) % period.y);

    float v00 = Hash01(Hash2(uint2(p00), seed));
    float v10 = Hash01(Hash2(uint2(p10), seed));
    float v01 = Hash01(Hash2(uint2(p01), seed));
    float v11 = Hash01(Hash2(uint2(p11), seed));

    float u = Fade(f.x);
    float v = Fade(f.y);
    return Lerp(Lerp(v00, v10, u), Lerp(v01, v11, u), v);
}

float PerlinNoise(float2 p, int2 period, uint seed, float rotation)
{
    int2 i = int2(floor(p));
    float2 f = frac(p);
    float2 u = float2(Fade(f.x), Fade(f.y));

    int2 p00 = int2((i.x % period.x + period.x) % period.x,
                    (i.y % period.y + period.y) % period.y);
    int2 p10 = int2(((i.x + 1) % period.x + period.x) % period.x,
                    (i.y % period.y + period.y) % period.y);
    int2 p01 = int2((i.x % period.x + period.x) % period.x,
                    ((i.y + 1) % period.y + period.y) % period.y);
    int2 p11 = int2(((i.x + 1) % period.x + period.x) % period.x,
                    ((i.y + 1) % period.y + period.y) % period.y);

    float2 g00 = GradientFromHash(Hash2(uint2(p00), seed), rotation);
    float2 g10 = GradientFromHash(Hash2(uint2(p10), seed), rotation);
    float2 g01 = GradientFromHash(Hash2(uint2(p01), seed), rotation);
    float2 g11 = GradientFromHash(Hash2(uint2(p11), seed), rotation);

    float n00 = dot(g00, f - float2(0.0f, 0.0f));
    float n10 = dot(g10, f - float2(1.0f, 0.0f));
    float n01 = dot(g01, f - float2(0.0f, 1.0f));
    float n11 = dot(g11, f - float2(1.0f, 1.0f));

    float x1 = Lerp(n00, n10, u.x);
    float x2 = Lerp(n01, n11, u.x);
    return 0.5f + 0.5f * Lerp(x1, x2, u.y);
}

float SimplexNoise(float2 p, int2 period, uint seed, float rotation)
{
    static const float F2 = 0.3660254037844386f;
    static const float G2 = 0.2113248654051871f;

    float s = (p.x + p.y) * F2;
    float2 i = floor(p + s);
    float t = (i.x + i.y) * G2;
    float2 X0 = i - t;
    float2 x0 = p - X0;

    int2 i1 = (x0.x > x0.y) ? int2(1, 0) : int2(0, 1);
    float2 x1 = x0 - float2(i1) + G2;
    float2 x2 = x0 - 1.0f + 2.0f * G2;

    int2 ii0 = int2((int(i.x) % period.x + period.x) % period.x,
                    (int(i.y) % period.y + period.y) % period.y);
    int2 ii1 = int2((int(i.x) + i1.x) % period.x, (int(i.y) + i1.y) % period.y);
    ii1 = int2((ii1.x + period.x) % period.x, (ii1.y + period.y) % period.y);
    int2 ii2 = int2((int(i.x) + 1) % period.x, (int(i.y) + 1) % period.y);
    ii2 = int2((ii2.x + period.x) % period.x, (ii2.y + period.y) % period.y);

    float2 g0 = GradientFromHash(Hash2(uint2(ii0), seed), rotation);
    float2 g1 = GradientFromHash(Hash2(uint2(ii1), seed), rotation);
    float2 g2 = GradientFromHash(Hash2(uint2(ii2), seed), rotation);

    float n0 = 0.0f;
    float n1 = 0.0f;
    float n2 = 0.0f;

    float t0 = 0.5f - dot(x0, x0);
    if (t0 > 0.0f) n0 = pow(t0, 4.0f) * dot(g0, x0);
    float t1 = 0.5f - dot(x1, x1);
    if (t1 > 0.0f) n1 = pow(t1, 4.0f) * dot(g1, x1);
    float t2 = 0.5f - dot(x2, x2);
    if (t2 > 0.0f) n2 = pow(t2, 4.0f) * dot(g2, x2);

    return 0.5f + 0.5f * (70.0f * (n0 + n1 + n2));
}

float WhiteNoise(float2 p, int2 period, uint seed)
{
    int2 i = int2(floor(p));
    int2 ii = int2((i.x % period.x + period.x) % period.x,
                   (i.y % period.y + period.y) % period.y);
    return Hash01(Hash2(uint2(ii), seed));
}

float VoronoiNoise(float2 p, int2 period, uint seed, uint mode, float jitter, float rotation)
{
    int2 base = int2(floor(p));
    float best = 1e9f;
    float second = 1e9f;
    float cellValue = 0.0f;

    for (int oy = -1; oy <= 1; ++oy)
    {
        for (int ox = -1; ox <= 1; ++ox)
        {
            int2 cell = base + int2(ox, oy);
            int2 wrapped = int2((cell.x % period.x + period.x) % period.x,
                                (cell.y % period.y + period.y) % period.y);
            uint h = Hash2(uint2(wrapped), seed);
            float2 r = float2(Hash01(HashU32(h ^ 0x68bc21ebu)),
                              Hash01(HashU32(h ^ 0x02e5be93u)));
            r = Rotate2(r - 0.5f, rotation) * jitter + 0.5f;
            float2 feature = float2(cell) + r;
            float d = length(feature - p);
            if (d < best)
            {
                second = best;
                best = d;
                cellValue = Hash01(h);
            }
            else if (d < second)
            {
                second = d;
            }
        }
    }

    if (mode == 1u)
    {
        return cellValue;
    }
    if (mode == 2u)
    {
        return saturate((second - best) * 4.0f);
    }
    return saturate(1.0f - best * 1.41421356f);
}

float GradientField(float2 uv, uint mode, float rotation)
{
    if (mode == 1u)
    {
        float2 center = float2(0.5f, 0.5f);
        float2 delta = abs(uv - center);
        delta = min(delta, 1.0f - delta);
        float dist = length(delta * 2.0f);
        return saturate(1.0f - dist);
    }

    float dir = rotation;
    float t = uv.x * cos(dir) + uv.y * sin(dir);
    return 0.5f + 0.5f * sin((t + 0.5f) * 6.28318530718f);
}

float ApplyBlend(float a, float b, uint blendMode, float weight)
{
    weight = saturate(weight);
    if (blendMode == 1u)
    {
        return lerp(a, a * b, weight);
    }
    if (blendMode == 2u)
    {
        float overlay = (a < 0.5f) ? (2.0f * a * b) : (1.0f - 2.0f * (1.0f - a) * (1.0f - b));
        return lerp(a, overlay, weight);
    }
    return saturate(a + b * weight);
}

float ApplyPost(float value)
{
    if ((g_Head1.z & 4u) != 0u)
    {
        value = clamp(value, g_Post0.z, g_Post0.w);
    }
    if ((g_Head1.z & 1u) != 0u)
    {
        float denom = max(g_Post0.y - g_Post0.x, 1e-6f);
        value = saturate((value - g_Post0.x) / denom);
    }
    if ((g_Head1.z & 2u) != 0u)
    {
        value = 1.0f - value;
    }
    if ((g_Head1.z & 8u) != 0u)
    {
        float denom = max(g_Post1.y - g_Post1.x, 1e-6f);
        float t = saturate((value - g_Post1.x) / denom);
        value = lerp(g_Post1.z, g_Post1.w, t);
    }
    value = pow(saturate(value), max(g_Primary2.x, 1e-6f));
    return saturate(value);
}

float SampleGenerator(uint kind, float2 p, int2 period, uint seed, float rotation, uint octaves, float lacunarity, float gain, uint voronoiMode, float jitter, uint gradientMode)
{
    if (kind == 4u)
    {
        return WhiteNoise(p, period, seed);
    }
    if (kind == 5u)
    {
        return ValueNoise(p, period, seed);
    }
    if (kind == 3u)
    {
        return VoronoiNoise(p, period, seed, voronoiMode, jitter, rotation);
    }
    if (kind == 6u)
    {
        return GradientField(frac(p / float2((float)period.x, (float)period.y)), gradientMode, rotation);
    }
    if (kind == 2u)
    {
        float sum = 0.0f;
        float amp = 1.0f;
        float norm = 0.0f;
        float2 freq = p;
        for (uint i = 0u; i < max(octaves, 1u); ++i)
        {
            sum += PerlinNoise(freq, period, seed + i * 31u, rotation) * amp;
            norm += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return (norm > 0.0f) ? (sum / norm) : 0.0f;
    }
    if (kind == 1u)
    {
        return SimplexNoise(p, period, seed, rotation);
    }
    return PerlinNoise(p, period, seed, rotation);
}

float SampleFinal(uint kind, float2 p, int2 period, uint seed, float rotation, uint octaves, float lacunarity, float gain, uint voronoiMode, float jitter, uint gradientMode)
{
    return SampleGenerator(kind, p, period, seed, rotation, octaves, lacunarity, gain, voronoiMode, jitter, gradientMode);
}

float SampleAt(float2 uv)
{
    int2 period = int2(max(1, (int)round(abs(g_Primary0.x))), max(1, (int)round(abs(g_Primary0.y))));
    float2 p = uv * float2(period) + g_Primary0.zw;
    float2 warp = float2(0.0f, 0.0f);
    if ((g_Head1.z & 32u) != 0u)
    {
        int2 warpPeriod = int2(max(1, (int)round(abs(g_Warp0.x))), max(1, (int)round(abs(g_Warp0.y))));
        float2 wp = uv * float2(warpPeriod) + g_Warp0.zw;
        float warpX = SampleGenerator(g_Secondary3.z, wp, warpPeriod, g_Secondary3.x, g_Warp1.x, 1u, g_Warp1.y, g_Warp1.z, g_Secondary2.y, g_Primary2.w, g_Secondary2.z);
        float warpY = SampleGenerator(g_Secondary3.z, wp + float2(19.13f, 7.91f), warpPeriod, g_Secondary3.x + 31u, g_Warp1.x, 1u, g_Warp1.y, g_Warp1.z, g_Secondary2.y, g_Primary2.w, g_Secondary2.z);
        warp = float2(warpX, warpY) * g_Primary2.z;
        p += warp * period;
    }

    float primary = SampleFinal(g_Head0.z, p, period, g_Secondary3.y, g_Primary1.x, g_Head1.w, g_Primary1.z, g_Primary1.w, g_Head0.w, g_Primary2.w, g_Head1.x);
    primary *= g_Primary1.y;

    if ((g_Head1.z & 16u) != 0u)
    {
        int2 secondaryPeriod = int2(max(1, (int)round(abs(g_Secondary0.x))), max(1, (int)round(abs(g_Secondary0.y))));
        float2 sp = uv * float2(secondaryPeriod) + g_Secondary0.zw;
        float secondary = SampleFinal(g_Secondary2.x, sp, secondaryPeriod, g_Secondary3.x, g_Secondary1.x, g_Secondary2.w, g_Secondary1.z, g_Secondary1.w, g_Secondary2.y, g_Primary2.w, g_Secondary2.z);
        secondary *= g_Secondary1.y;
        primary = ApplyBlend(primary, secondary, g_Head1.y, g_Primary2.y);
    }

    return ApplyPost(primary);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_Head0.x || id.y >= g_Head0.y)
    {
        return;
    }

    float2 uv = (float2(id.xy) + 0.5f) / float2(max(1u, g_Head0.x), max(1u, g_Head0.y));
    float value = SampleAt(uv);
    OutTex[id.xy] = float4(value, value, value, 1.0f);
}
)";
}
