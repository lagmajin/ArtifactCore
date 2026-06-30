module;
#include <algorithm>
#include <cmath>

module ImageProcessing;
import :LeaveColor;

namespace ArtifactCore {

static float colorDist(const float3& a, const float3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz) * 0.57735f;
}

static float luma(const float3& c) {
    return c.x * 0.299f + c.y * 0.587f + c.z * 0.114f;
}

void LeaveColor::process(float4* buffer, int width, int height, const LeaveColorSettings& s) {
    float3 kc = {s.keyColor.x, s.keyColor.y, s.keyColor.z};
    for (int i = 0; i < width * height; ++i) {
        auto& px = buffer[i];
        float3 c = {px.x, px.y, px.z};
        float dist = colorDist(c, kc);
        float mix = std::clamp((dist - s.tolerance) / std::max(s.softness, 0.001f), 0.0f, 1.0f);
        float gray = luma(c);
        px.x = c.x + (gray - c.x) * mix * s.desaturateAmount;
        px.y = c.y + (gray - c.y) * mix * s.desaturateAmount;
        px.z = c.z + (gray - c.z) * mix * s.desaturateAmount;
    }
}

void LeaveColor::process(ImageF32x4_RGBA& image, const LeaveColorSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
