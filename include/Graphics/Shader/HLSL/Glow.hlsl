Texture2D<float4> InputTexture  : register(t0); // 入力テクスチャ (RGBA順, float4形式)
RWTexture2D<float4> OutputTexture : register(u0); 

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}