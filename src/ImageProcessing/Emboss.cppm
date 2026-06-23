module;
#include <algorithm>
#include <cmath>

module ImageProcessing:Emboss;

namespace ArtifactCore {

void Emboss::process(float4* buffer, int width, int height, const EmbossSettings& s) {
    auto* tmp = new float4[width * height];
    std::copy_n(buffer, width * height, tmp);
    static constexpr float k[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    static constexpr float t[3][3] = {{1,2,1},{0,0,0},{-1,-2,-1}};
    float ca = std::cos(s.angle), sa = std::sin(s.angle);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float gx = 0, gy = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int cx = std::clamp(x + dx, 0, width - 1);
                    int cy = std::clamp(y + dy, 0, height - 1);
                    float l = tmp[cy * width + cx].x * 0.299f
                            + tmp[cy * width + cx].y * 0.587f
                            + tmp[cy * width + cx].z * 0.114f;
                    gx += l * k[dy + 1][dx + 1];
                    gy += l * t[dy + 1][dx + 1];
                }
            }
            float edge = gx * ca + gy * sa;
            auto& src = tmp[y * width + x];
            auto& dst = buffer[y * width + x];
            dst.x = std::clamp(src.x + edge * s.intensity, 0.0f, 1.0f);
            dst.y = std::clamp(src.y + edge * s.intensity, 0.0f, 1.0f);
            dst.z = std::clamp(src.z + edge * s.intensity, 0.0f, 1.0f);
            dst.w = src.w;
        }
    }
    delete[] tmp;
}

void Emboss::process(ImageF32x4_RGBA& image, const EmbossSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
