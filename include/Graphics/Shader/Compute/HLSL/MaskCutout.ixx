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
    float3 _pad;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint outWidth = 0;
    uint outHeight = 0;
    OutTex.GetDimensions(outWidth, outHeight);
    if (id.x >= outWidth || id.y >= outHeight) {
        return;
    }

    float4 scene = SceneTex[id.xy];
    float maskAlpha = MaskTex[id.xy].a;
    float cutout = saturate(maskAlpha * opacity);
    OutTex[id.xy] = float4(scene.rgb * cutout, scene.a * cutout);
}
)");

} // namespace ArtifactCore
