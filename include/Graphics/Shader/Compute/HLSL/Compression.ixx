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
    uint g_Pad0;
    uint g_Pad1;
};

// Input: RGBA16_FLOAT texture (linear space)
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

// Compression buffer (simplified - just copy for now)
RWStructuredBuffer<uint> g_CompressedData : register(u1);
StructuredBuffer<uint> g_DecompressedData : register(t1);

// Compress: Texture -> Compressed Buffer
[numthreads(64, 1, 1)]
void CompressCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint blockId = dispatchThreadID.x;
    if (blockId >= g_NumBlocks) return;

    // Simple LZ4-style: find repeated sequences
    // For now: just copy data (TODO: implement actual compression)
    uint dataStart = blockId * g_BlockSize / 4; // uint4 units

    // Read texture data in blocks
    for (uint i = 0; i < g_BlockSize / 16; ++i) { // 16 bytes per iteration
        uint idx = dataStart + i * 4;
        if (idx >= g_NumBlocks * g_BlockSize / 4) break;

        // Pack float4 to uint4
        float4 data = g_InputTexture.Load(int3(idx % 1024, idx / 1024, 0));
        uint packed = (uint(data.x * 65535.0f) & 0xFFFF) |
                     ((uint(data.y * 65535.0f) & 0xFFFF) << 16);
        g_CompressedData[idx] = packed;
    }
}

// Decompress: Compressed Buffer -> Texture
[numthreads(64, 1, 1)]
void DecompressCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint blockId = dispatchThreadID.x;
    if (blockId >= g_NumBlocks) return;

    uint dataStart = blockId * g_BlockSize / 4;

    for (uint i = 0; i < g_BlockSize / 16; ++i) {
        uint idx = dataStart + i * 4;
        if (idx >= g_NumBlocks * g_BlockSize / 4) break;

        uint packed = g_DecompressedData[idx];
        float x = (packed & 0xFFFF) / 65535.0f;
        float y = ((packed >> 16) & 0xFFFF) / 65535.0f;

        g_OutputTexture[int2(idx % 1024, idx / 1024)] = float4(x, y, 0.0f, 1.0f);
    }
}
)";

    inline constexpr const char* CompressEntryPoint = "CompressCS";
    inline constexpr const char* DecompressEntryPoint = "DecompressCS";
}
