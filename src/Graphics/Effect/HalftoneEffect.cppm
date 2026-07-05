module;
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.Halftone;

import Channel;

namespace ArtifactCore {

HalftoneEffect::HalftoneEffect() {
    parameters_.push_back({"Size", "Dot Size", EffectParameterType::Float, 12.0f, 2.0f, 50.0f});
    parameters_.push_back({"Angle", "Grid Angle", EffectParameterType::Float, 0.785f, -3.1415f, 3.1415f});
    parameters_.push_back({"Contrast", "Intensity", EffectParameterType::Float, 1.0f, 0.0f, 2.0f});
}

void HalftoneEffect::process(VideoFrame& frame, const CreativeEffectContext&) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);

    if (!r_ch || !g_ch || !b_ch) return;

    const int w = frame.width();
    const int h = frame.height();

    const float cellSize = dotSize();
    const float ang = dotAngle();
    const float cosA = std::cos(ang);
    const float sinA = std::sin(ang);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int idx = y * w + x;

            const float lum = 0.299f * r_ch->data()[idx] + 0.587f * g_ch->data()[idx] + 0.114f * b_ch->data()[idx];
            const float rx = (float)x * cosA - (float)y * sinA;
            const float ry = (float)x * sinA + (float)y * cosA;

            float u = std::fmod(rx, cellSize);
            if (u < 0) u += cellSize;
            float v = std::fmod(ry, cellSize);
            if (v < 0) v += cellSize;

            const float du = u - cellSize * 0.5f;
            const float dv = v - cellSize * 0.5f;
            const float dist = std::sqrt(du * du + dv * dv);
            const float maxR = cellSize * 0.5f * 1.414f;
            const float dotRadius = maxR * std::sqrt(std::max(0.0f, 1.0f - lum * contrast()));

            const float smoothFactor = 1.0f;
            const float val = std::clamp((dist - dotRadius) / smoothFactor + 0.5f, 0.0f, 1.0f);

            r_ch->data()[idx] = val;
            g_ch->data()[idx] = val;
            b_ch->data()[idx] = val;
        }
    }
}

}
