module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.Compression;

export namespace ArtifactCore::Shaders::Compression
{
    inline constexpr const char* CompressSource = R"(
// GPUCompression.hlsl - Simple LZ4-style GPU compression
// Block-based parallel compression for texture data

cbuffer CompressionCB : register(b0)
{
    uint g_BlockSize;
    uint g_NumBlocks;
    uint g_Width;
    uint g_Height;
};

// Input: RGBA16_FLOAT texture (linear space)
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

// Compression buffer: two packed half-float words per RGBA pixel.
RWStructuredBuffer<uint> g_CompressedData : register(u1);
StructuredBuffer<uint> g_DecompressedData : register(t1);

// Compress: Texture -> Compressed Buffer
[numthreads(64, 1, 1)]
void CompressCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint blockId = dispatchThreadID.x;
    if (blockId >= g_NumBlocks) return;

    uint dataStart = blockId * g_BlockSize / 4;

    // Read texture data in blocks
    for (uint i = 0; i < g_BlockSize / 8; ++i) { // 8 bytes per RGBA16 pixel
        uint idx = dataStart + i * 2;
        if (idx + 1 >= g_NumBlocks * g_BlockSize / 4) break;

        uint pixel = idx / 2;
        if (pixel >= g_Width * g_Height) return;
        float4 data = g_InputTexture.Load(int3(pixel % g_Width, pixel / g_Width, 0));
        g_CompressedData[idx] = f32tof16(data.x) | (f32tof16(data.y) << 16);
        g_CompressedData[idx + 1] = f32tof16(data.z) | (f32tof16(data.w) << 16);
    }
}

// Decompress: Compressed Buffer -> Texture
[numthreads(64, 1, 1)]
void DecompressCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint blockId = dispatchThreadID.x;
    if (blockId >= g_NumBlocks) return;

    uint dataStart = blockId * g_BlockSize / 4;

    for (uint i = 0; i < g_BlockSize / 8; ++i) {
        uint idx = dataStart + i * 2;
        if (idx + 1 >= g_NumBlocks * g_BlockSize / 4) break;

        uint packedXY = g_DecompressedData[idx];
        uint packedZW = g_DecompressedData[idx + 1];
        float x = f16tof32(packedXY & 0xFFFF);
        float y = f16tof32(packedXY >> 16);
        float z = f16tof32(packedZW & 0xFFFF);
        float w = f16tof32(packedZW >> 16);

        uint pixel = idx / 2;
        if (pixel >= g_Width * g_Height) return;
        g_OutputTexture[int2(pixel % g_Width, pixel / g_Width)] = float4(x, y, z, w);
    }
}
)";

    inline constexpr const char* CompressEntryPoint = "CompressCS";
    inline constexpr const char* DecompressEntryPoint = "DecompressCS";
}
