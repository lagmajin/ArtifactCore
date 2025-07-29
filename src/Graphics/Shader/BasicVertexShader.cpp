module;
#include <QString>
#include <QByteArray>
module Graphics.Shader.Basics.Vertex;



namespace ArtifactCore
{
 const QByteArray lineShaderVSText = R"(
cbuffer CameraMatrices : register(b0)
{
    float4x4 WorldMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
 float LineThickness;
};

struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(float4(input.position, 1.0), WorldMatrix);
    float4 viewPos = mul(worldPos, ViewMatrix);
    output.position = mul(viewPos, ProjMatrix);

    return output;
}

)";













}