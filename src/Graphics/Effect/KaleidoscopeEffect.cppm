module;
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.Kaleidoscope;

import Channel;

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

    for (int y = 0; y < h; ++y) {
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
            const int idx = y * w + x;
            const int sidx = isy * w + isx;

            r_ch->data()[idx] = r_old[sidx];
            g_ch->data()[idx] = g_old[sidx];
            b_ch->data()[idx] = b_old[sidx];
        }
    }
}

}
