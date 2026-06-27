module;
#include <utility>
export module Graphics.Shader.Compute.HLSL.MaskPathRasterizer;

#include <QByteArray>

export namespace ArtifactCore {

inline const QByteArray maskPathRasterizerShaderText = QByteArray(R"(
struct Segment {
    float2 startPos;
    float2 endPos;
};

StructuredBuffer<Segment> Segments : register(t0);
RWTexture2D<unorm float4> OutMask  : register(u0);

cbuffer RasterizerParams : register(b0)
{
    uint  numSegments;
    uint  maskMode;
    uint  inverted;
    float featherPixels;
    int   outputWidth;
    int   outputHeight;
    uint2 _pad0;
};

bool rayIntersectSegment(float2 origin, float2 a, float2 b)
{
    if ((a.y > origin.y) == (b.y > origin.y)) {
        return false;
    }
    float t = (origin.y - a.y) / (b.y - a.y);
    if (t < 0.0 || t > 1.0) {
        return false;
    }
    float xIntersect = a.x + t * (b.x - a.x);
    return xIntersect > origin.x;
}

float pointToSegmentDistSq(float2 p, float2 a, float2 b)
{
    float2 ab = b - a;
    float2 ap = p - a;
    float t = dot(ap, ab) / max(dot(ab, ab), 1e-8);
    t = saturate(t);
    float2 closest = a + t * ab;
    return dot(p - closest, p - closest);
}

struct InsideResult {
    float inside;
    float minDistSq;
};

InsideResult evaluateInside(float2 uv, uint count)
{
    InsideResult res;
    res.inside = 0.0;
    res.minDistSq = 1e8;
    uint crossings = 0;
    for (uint i = 0; i < count; i++) {
        Segment seg = Segments[i];
        float d = pointToSegmentDistSq(uv, seg.startPos, seg.endPos);
        if (d < res.minDistSq) { res.minDistSq = d; }
        if (rayIntersectSegment(uv, seg.startPos, seg.endPos)) {
            crossings++;
        }
    }
    res.inside = (crossings & 1) == 1 ? 1.0 : 0.0;
    return res;
}

float featherAlpha(float inside, float minDistSq, float feather)
{
    float dist = sqrt(minDistSq);

    [branch]
    if (inside > 0.5) {
        float falloff = saturate(dist / max(feather, 0.5));
        return lerp(0.5, 1.0, falloff);
    } else {
        float falloff = saturate(dist / max(feather, 0.5));
        return lerp(0.5, 0.0, falloff);
    }
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= (uint)outputWidth || id.y >= (uint)outputHeight) {
        return;
    }

    float2 pixelCenter = float2(id.x + 0.5, id.y + 0.5);

    float2 offsets[4] = {
        float2(-0.25, -0.25),
        float2(0.25, -0.25),
        float2(-0.25, 0.25),
        float2(0.25, 0.25)
    };

    float sum = 0.0;
    for (int s = 0; s < 4; s++) {
        float2 samplePos = pixelCenter + offsets[s];
        InsideResult ir = evaluateInside(samplePos, numSegments);

        float a;
        if (featherPixels > 0.5) {
            a = featherAlpha(ir.inside, ir.minDistSq, featherPixels);
        } else {
            a = ir.inside;
        }
        sum += a;
    }
    float alpha = sum / 4.0;

    if (inverted != 0) {
        alpha = 1.0 - alpha;
    }

    OutMask[id.xy] = float4(alpha, alpha, alpha, alpha);
}
)");

} // namespace ArtifactCore
