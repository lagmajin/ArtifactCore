Texture2D<float4> SrcTex  : register(t0);
Texture2D<float4> DstTex  : register(t1);
RWTexture2D<float4> ResultTex : register(u0);
float hash(uint2 p) { return frac(sin(dot(float2(p), float2(12.9898,78.233)))*43758.5453); }
[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));
    float rnd = hash(DTid.xy);
    float3 result = (rnd < src.a) ? src.rgb : dst.rgb;
    ResultTex[DTid.xy] = float4(result, 1.0);
}
