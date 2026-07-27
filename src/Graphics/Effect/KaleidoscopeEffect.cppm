module;
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.Kaleidoscope;

import Channel;
import Core.Parallel;

namespace ArtifactCore {

KaleidoscopeEffect::KaleidoscopeEffect() {
    parameters_.push_back({"Count", "Divisions", EffectParameterType::Float, 6.0f, 1.0f, 24.0f});
    parameters_.push_back({"Angle", "Rotation", EffectParameterType::Float, 0.0f, -3.1415f, 3.1415f});
    parameters_.push_back({"CenterX", "Center X", EffectParameterType::Float, 0.5f, 0.0f, 1.0f});
    parameters_.push_back({"CenterY", "Center Y", EffectParameterType::Float, 0.5f, 0.0f, 1.0f});
}

void KaleidoscopeEffect::process(VideoFrame& frame, const CreativeEffectContext&) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);
    if (!r_ch || !g_ch || !b_ch) return;

    const int w = frame.width();
    const int h = frame.height();

    std::vector<float> r_old(r_ch->data(), r_ch->data() + r_ch->size());
    std::vector<float> g_old(g_ch->data(), g_ch->data() + g_ch->size());
    std::vector<float> b_old(b_ch->data(), b_ch->data() + b_ch->size());

    const float cx = centerX() * w;
    const float cy = centerY() * h;
    const float divCount = std::max(1.0f, count());
    const float baseAngle = angle();
    const float segmentAngle = (2.0f * 3.14159265f) / divCount;
    float* rData = r_ch->data();
    float* gData = g_ch->data();
    float* bData = b_ch->data();

    Parallel::For(0, h, w * h, [&](int y) {
        float* rRow = rData + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        float* gRow = gData + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        float* bRow = bData + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        for (int x = 0; x < w; ++x) {
            const float dx = x - cx;
            const float dy = y - cy;

            const float radius = std::sqrt(dx * dx + dy * dy);
            float theta = std::atan2(dy, dx) + baseAngle;
            while (theta < 0) theta += 2.0f * 3.14159265f;

            float modTheta = std::fmod(theta, segmentAngle);
            if (modTheta > segmentAngle * 0.5f) {
                modTheta = segmentAngle - modTheta;
            }

            const float sx = cx + radius * std::cos(modTheta);
            const float sy = cy + radius * std::sin(modTheta);

            const int isx = std::clamp((int)sx, 0, w - 1);
            const int isy = std::clamp((int)sy, 0, h - 1);
            const int sidx = isy * w + isx;

            rRow[x] = r_old[sidx];
            gRow[x] = g_old[sidx];
            bRow[x] = b_old[sidx];
        }
    });
}

}
