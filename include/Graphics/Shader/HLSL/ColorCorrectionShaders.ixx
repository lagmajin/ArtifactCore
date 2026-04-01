module;
#include <QString>
#include <QByteArray>
#include "..\..\..\Define\DllExportMacro.hpp"

export module Graphics.Shader.HLSL.ColorCorrection;

export namespace ArtifactCore {

// ─────────────────────────────────────────────────────────
// Color Correction Shaders (HLSL)
//
// Shared per-pixel color correction operations:
//   - Exposure (EV * 2^exposure + offset, gamma)
//   - Brightness/Contrast/Highlights/Shadows
//   - Hue/Saturation/Lightness/Colorize
// ─────────────────────────────────────────────────────────

LIBRARY_DLL_API const QByteArray g_colorCorrectionVS = R"(
struct VSInput {
    float2 pos     : ATTRIB0;
    float2 texCoord: TEXCOORD0;
};

struct PSInput {
    float4 pos     : SV_POSITION;
    float2 texCoord: TEXCOORD0;
};

PSInput main(VSInput input) {
    PSInput output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.texCoord = input.texCoord;
    return output;
}
)";

// Exposure PS
LIBRARY_DLL_API const QByteArray g_exposurePS = R"(
Texture2D    g_texture  : register(t0);
SamplerState g_sampler  : register(s0);

cbuffer ColorCorrectionCB : register(b0) {
    float exposureMultiplier;
    float offset;
    float gammaInv;
    float padding;
};

float4 main(PSInput input) : SV_TARGET {
    float4 color = g_texture.Sample(g_sampler, input.texCoord);

    for (int c = 0; c < 3; ++c) {
        float val = color[c] * exposureMultiplier;
        val += offset;
        val = max(0.0f, val);
        if (gammaInv != 1.0f) {
            val = pow(val, gammaInv);
        }
        color[c] = clamp(val, 0.0f, 1.0f);
    }

    return color;
}
)";

// Brightness/Contrast PS
LIBRARY_DLL_API const QByteArray g_brightnessContrastPS = R"(
Texture2D    g_texture  : register(t0);
SamplerState g_sampler  : register(s0);

cbuffer ColorCorrectionCB : register(b0) {
    float brightness;
    float contrastFactor;
    float highlights;
    float shadows;
};

float4 main(PSInput input) : SV_TARGET {
    float4 color = g_texture.Sample(g_sampler, input.texCoord);

    for (int c = 0; c < 3; ++c) {
        float val = color[c];

        // 1. Brightness
        val += brightness;

        // 2. Contrast (centered at 0.5)
        val = contrastFactor * (val - 0.5f) + 0.5f;

        // 3. Highlights (bright areas only)
        if (val > 0.5f) {
            float highlightWeight = (val - 0.5f) * 2.0f;
            val += highlights * highlightWeight * 0.5f;
        }

        // 4. Shadows (dark areas only)
        if (val < 0.5f) {
            float shadowWeight = (0.5f - val) * 2.0f;
            val += shadows * shadowWeight * 0.5f;
        }

        color[c] = clamp(val, 0.0f, 1.0f);
    }

    return color;
}
)";

// Hue/Saturation PS
LIBRARY_DLL_API const QByteArray g_hueSaturationPS = R"(
Texture2D    g_texture  : register(t0);
SamplerState g_sampler  : register(s0);

cbuffer ColorCorrectionCB : register(b0) {
    float hueShift;
    float saturationScale;
    float lightnessShift;
    int   colorize;
};

float3 rgb2hsv(float3 c) {
    float4 K = float4(0.0f, -1.0f/3.0f, 2.0f/3.0f, -1.0f);
    float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10f;
    return float3(abs(q.z + (q.w - q.y) / (6.0f * d + e)), d / (q.x + e), q.x);
}

float3 hsv2rgb(float3 c) {
    float4 K = float4(1.0f, 2.0f/3.0f, 1.0f/3.0f, 3.0f);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0f - K.www);
    return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0f, 1.0f), c.y);
}

float4 main(PSInput input) : SV_TARGET {
    float4 color = g_texture.Sample(g_sampler, input.texCoord);
    float3 hsv = rgb2hsv(color.rgb);

    if (colorize) {
        hsv.x = fmod(hueShift + 360.0f, 360.0f) / 360.0f;
        hsv.y = saturationScale;
        hsv.z = clamp(hsv.z + lightnessShift, 0.0f, 1.0f);
    } else {
        hsv.x = fmod(hsv.x + hueShift / 360.0f + 1.0f, 1.0f);
        hsv.y = clamp(hsv.y * saturationScale, 0.0f, 1.0f);
        hsv.z = clamp(hsv.z + lightnessShift, 0.0f, 1.0f);
    }

    color.rgb = hsv2rgb(hsv);
    return color;
}
)";

// Lift/Gamma/Gain PS
LIBRARY_DLL_API const QByteArray g_liftGammaGainPS = R"(
Texture2D    g_texture  : register(t0);
SamplerState g_sampler  : register(s0);

cbuffer ColorCorrectionCB : register(b0) {
    float liftR, liftG, liftB, padding1;
    float gammaR, gammaG, gammaB, padding2;
    float gainR, gainG, gainB, padding3;
};

float4 main(PSInput input) : SV_TARGET {
    float4 color = g_texture.Sample(g_sampler, input.texCoord);

    // 1. Lift: shadow offset
    color.r += liftR * 0.1f;
    color.g += liftG * 0.1f;
    color.b += liftB * 0.1f;

    // 2. Gamma: midtone power
    color.r = pow(max(color.r, 0.0f), 1.0f / gammaR);
    color.g = pow(max(color.g, 0.0f), 1.0f / gammaG);
    color.b = pow(max(color.b, 0.0f), 1.0f / gammaB);

    // 3. Gain: highlight multiplier
    color.r *= gainR;
    color.g *= gainG;
    color.b *= gainB;

    color.rgb = clamp(color.rgb, 0.0f, 1.0f);
    return color;
}
)";

// White Balance PS
LIBRARY_DLL_API const QByteArray g_whiteBalancePS = R"(
Texture2D    g_texture  : register(t0);
SamplerState g_sampler  : register(s0);

cbuffer ColorCorrectionCB : register(b0) {
    float corrR, corrG, corrB, tintFactor;
    float brightnessMul, padding1, padding2, padding3;
};

float4 main(PSInput input) : SV_TARGET {
    float4 color = g_texture.Sample(g_sampler, input.texCoord);

    // Temperature correction
    color.r *= corrR;
    color.g *= corrG;
    color.b *= corrB;

    // Tint (green ←→ magenta)
    float tintG = 1.0f + tintFactor * 0.5f;
    float tintM = 1.0f - tintFactor * 0.5f;
    color.g *= tintG;
    color.r *= tintM;
    color.b *= tintM;

    // Brightness
    color.rgb *= brightnessMul;

    color.rgb = clamp(color.rgb, 0.0f, 1.0f);
    return color;
}
)";

} // namespace ArtifactCore
