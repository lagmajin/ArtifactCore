// GPU Block-Matching Motion Estimation
// Computes per-pixel motion vectors between two frames
// Output: float2 (dx, dy) per pixel

Texture2D<float4> g_PrevFrame : register(t0);
Texture2D<float4> g_CurrFrame : register(t1);
RWTexture2D<float2> g_OutputVectors : register(u0);

cbuffer MotionVectorParams : register(b0) {
    int g_BlockSize;        // block size in pixels (e.g., 8)
    int g_SearchRadius;     // search radius in pixels (e.g., 16)
    int g_StepSize;         // pixel step between blocks (e.g., 4)
    float g_Lambda;         // smoothness weight for hierarchical refinement
    int g_Width;
    int g_Height;
    int g_Pad0;
    int g_Pad1;
};

// Luminance from linear float4
float luminance(float4 c) {
    return dot(c.rgb, float3(0.299, 0.587, 0.114));
}

// Sum of Absolute Differences for a block at positions (bx, by) in prev vs (bx+mvx, by+mvy) in curr
float computeSAD(int bx, int by, int mvx, int mvy, int blockSize) {
    float sad = 0.0;
    for (int dy = 0; dy < blockSize; ++dy) {
        for (int dx = 0; dx < blockSize; ++dx) {
            int px = bx + dx;
            int py = by + dy;
            int qx = bx + dx + mvx;
            int qy = by + dy + mvy;
            if (px >= 0 && px < g_Width && py >= 0 && py < g_Height &&
                qx >= 0 && qx < g_Width && qy >= 0 && qy < g_Height) {
                float lp = luminance(g_PrevFrame[uint2(px, py)]);
                float lq = luminance(g_CurrFrame[uint2(qx, qy)]);
                sad += abs(lp - lq);
            }
        }
    }
    return sad;
}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    // This thread handles output pixel (DTid.x, DTid.y)
    // Convert pixel position to block-aligned search position
    int bx = (DTid.x / g_StepSize) * g_StepSize;
    int by = (DTid.y / g_StepSize) * g_StepSize;
    
    if (bx >= g_Width || by >= g_Height) return;
    
    // Full search within radius
    float bestSAD = 1e20;
    int bestMvX = 0;
    int bestMvY = 0;
    
    int r = g_SearchRadius;
    for (int my = -r; my <= r; ++my) {
        for (int mx = -r; mx <= r; ++mx) {
            float sad = computeSAD(bx, by, mx, my, g_BlockSize);
            // small bias toward zero motion
            float distCost = (float)(mx * mx + my * my) * 0.001;
            if (sad + distCost < bestSAD) {
                bestSAD = sad + distCost;
                bestMvX = mx;
                bestMvY = my;
            }
        }
    }
    
    // Write per-pixel motion vector (same for all pixels in this block region)
    float2 mv = float2((float)bestMvX, (float)bestMvY);
    g_OutputVectors[DTid.xy] = mv;
}
