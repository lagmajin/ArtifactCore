// BoxBlur5x5_CS.hlsl
Texture2D<float4> InputTex : register(t0);
RWTexture2D<float4> OutputTex : register(u0);

SamplerState samplerLinear : register(s0);

#define KERNEL_RADIUS 2  // 5x5 = 2ピクセルずつ上下左右に拡張
#define KERNEL_SIZE (2 * KERNEL_RADIUS + 1)

[numthreads(16,16,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int2 pix = int2(DTid.xy);
    int2 size;
    InputTex.GetDimensions(size.x, size.y);

    float4 sum = float4(0,0,0,0);

    // 周囲5x5ピクセルの単純平均
    for (int y = -KERNEL_RADIUS; y <= KERNEL_RADIUS; ++y)
    {
        for (int x = -KERNEL_RADIUS; x <= KERNEL_RADIUS; ++x)
        {
            int2 coord = pix + int2(x, y);
            // 境界はclamp
            coord = clamp(coord, int2(0,0), size - 1);
            sum += InputTex.Load(int3(coord, 0));
        }
    }

    float kernelArea = (float)(KERNEL_SIZE * KERNEL_SIZE);
    float4 blurred = sum / kernelArea;

    OutputTex[pix] = blurred;
}