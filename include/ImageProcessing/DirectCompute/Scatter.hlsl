Texture2D<float4> InputTexture  : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);
Texture2D<float4> NoiseTexture  : register(t1);

cbuffer Params : register(b0)
{
    float Amount;
    int Seed;
    float2 _pad;
};

uint hash(uint s) {
    s = s * 747796405 + 2891336453;
    s = ((s >> ((s >> 28) + 4)) ^ s) * 277803737;
    return (s >> 22) ^ s;
}

float2 randOffset(uint2 pos, uint seed) {
    uint h = hash(hash(pos.x + seed * 7919) + pos.y * 6271);
    return float2(float(h & 0xFFFF), float((h >> 16) & 0xFFFF)) / 65535.0f * 2.0f - 1.0f;
}

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);
    if (ID.x >= width || ID.y >= height) return;

    float2 offset = randOffset(ID.xy, Seed) * Amount;
    int2 srcCoord = int2(ID.x + int(offset.x), ID.y + int(offset.y));
    srcCoord = clamp(srcCoord, int2(0, 0), int2(width - 1, height - 1));
    OutputTexture[ID.xy] = InputTexture[uint2(srcCoord)];
}
