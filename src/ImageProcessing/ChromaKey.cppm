module;
#include <algorithm>
#include <cmath>

module ImageProcessing:ChromaKey;

namespace ArtifactCore {

static float colorDist(const float3& a, const float3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz) * 0.57735f;
}

void ChromaKey::process(float4* buffer, int width, int height, const ChromaKeySettings& s) {
    float3 kc = {s.keyColor.x, s.keyColor.y, s.keyColor.z};
    for (int i = 0; i < width * height; ++i) {
        auto& px = buffer[i];
        float3 c = {px.x, px.y, px.z};
        float dist = colorDist(c, kc);
        float alpha = std::clamp((dist - s.tolerance) / std::max(s.softness, 0.001f), 0.0f, 1.0f);
        px.w = px.w * (1.0f - alpha);
    }
}

void ChromaKey::process(ImageF32x4_RGBA& image, const ChromaKeySettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
