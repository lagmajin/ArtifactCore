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

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(float4(input.position, 1.0), WorldMatrix);
    float4 viewPos = mul(worldPos, ViewMatrix);
    output.position = mul(viewPos, ProjMatrix);

    return output;
}