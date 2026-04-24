module;
#include <utility>
export module Graphics.Shader.Compute.HLSL.MaskCutout;
#include <QByteArray>

export namespace ArtifactCore {

inline const QByteArray maskCutoutShaderText = QByteArray(R"(
Texture2D<float4> SceneTex : register(t0);
Texture2D<float4> MaskTex  : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer MaskParams : register(b0)
{
    float opacity;
    uint matteMode;
    uint luminanceStandard;
    uint _pad;
};

float3 getLuminanceWeights(uint standard)
{
    if (standard == 0) {
        return float3(0.2990f, 0.5870f, 0.1140f);
    }
    if (standard == 2) {
        return float3(0.2627f, 0.6780f, 0.0593f);
    }
    return float3(0.2126f, 0.7152f, 0.0722f);
}

float sampleMatte(float4 matte, uint mode, uint standard)
{
    if (mode == 0) {
        return 1.0f;
    }

    float value = 1.0f;
    if (mode == 1 || mode == 2) {
        value = matte.a;
        if (mode == 2) {
            value = 1.0f - value;
        }
    } else {
        value = dot(matte.rgb, getLuminanceWeights(standard));
        if (mode == 4) {
            value = 1.0f - value;
        }
    }

    return saturate(value);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint outWidth = 0;
    uint outHeight = 0;
    uint matteWidth = 0;
    uint matteHeight = 0;
    OutTex.GetDimensions(outWidth, outHeight);
    MaskTex.GetDimensions(matteWidth, matteHeight);
    if (id.x >= outWidth || id.y >= outHeight || outWidth == 0 || outHeight == 0) {
        return;
    }

    float4 scene = SceneTex[id.xy];
    if (matteMode == 0) {
        OutTex[id.xy] = scene;
        return;
    }

    uint matteX = min((id.x * matteWidth) / outWidth, matteWidth > 0 ? (matteWidth - 1) : 0);
    uint matteY = min((id.y * matteHeight) / outHeight, matteHeight > 0 ? (matteHeight - 1) : 0);
    float4 matte = MaskTex[uint2(matteX, matteY)];
    float cutout = saturate(sampleMatte(matte, matteMode, luminanceStandard) * opacity);
    OutTex[id.xy] = float4(scene.rgb * cutout, scene.a * cutout);
}
)");

} // namespace ArtifactCore
