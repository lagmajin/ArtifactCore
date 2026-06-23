Texture2D<float4> InputTexture  : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    int Radius;
};

// 3x3 median using sorting network (Batcher's odd-even mergesort)
void sort3(inout float a[9]) {
    // Bitonic sort for 9 elements
    #define SWAP(i, j) { if (a[i] > a[j]) { float t = a[i]; a[i] = a[j]; a[j] = t; } }
    SWAP(0, 1); SWAP(2, 3); SWAP(4, 5); SWAP(6, 7);
    SWAP(0, 2); SWAP(1, 3); SWAP(4, 6); SWAP(5, 7);
    SWAP(1, 2); SWAP(5, 6); SWAP(0, 4); SWAP(3, 7);
    SWAP(1, 5); SWAP(2, 6); SWAP(1, 4); SWAP(3, 5);
    SWAP(2, 4); SWAP(3, 4);
    #undef SWAP
}

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);
    if (ID.x >= width || ID.y >= height) return;

    float r[9], g[9], b[9], a[9];
    int idx = 0;
    [unroll]
    for (int dy = -1; dy <= 1; ++dy) {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx) {
            uint2 coord = ID.xy + uint2(dx, dy);
            coord.x = min(max(coord.x, 0), width - 1);
            coord.y = min(max(coord.y, 0), height - 1);
            float4 c = InputTexture[coord];
            r[idx] = c.r; g[idx] = c.g; b[idx] = c.b; a[idx] = c.a;
            ++idx;
        }
    }

    sort3(r); sort3(g); sort3(b); sort3(a);
    OutputTexture[ID.xy] = float4(r[4], g[4], b[4], a[4]);
}
