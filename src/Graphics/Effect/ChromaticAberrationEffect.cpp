module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

module Graphics.Effect.Creative.ChromaticAberration;

import Graphics.Effect.Creative;
import Channel;
import Core.Parallel;

namespace ArtifactCore {

ChromaticAberrationEffect::ChromaticAberrationEffect() {
    parameters_.push_back({"Amount", "Amount", EffectParameterType::Float, 1.0f, 0.0f, 50.0f});
    parameters_.push_back({"Angle", "Angle", EffectParameterType::Float, 0.0f, -180.0f, 180.0f});
}

void ChromaticAberrationEffect::process(VideoFrame& frame, const CreativeEffectContext&) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);
    if (!r_ch || !g_ch || !b_ch) return;

    const float amt = std::max(0.0f, amount());
    const float ang = angle() * 3.14159265f / 180.0f;
    const float cosA = std::cos(ang);
    const float sinA = std::sin(ang);
    const int shiftR_x = static_cast<int>(std::round(amt * cosA));
    const int shiftR_y = static_cast<int>(std::round(amt * sinA));
    const int shiftB_x = -shiftR_x;
    const int shiftB_y = -shiftR_y;
    // Subpixel values can quantize to an identity shift; avoid full-frame scratch work.
    if (shiftR_x == 0 && shiftR_y == 0) return;

    const int w = frame.width();
    const int h = frame.height();

    auto shift_channel = [&](float* dst, const float* src, int sx, int sy) {
        Parallel::ForPixels(0, h, w, h, [&](int y) {
            float* dstRow = dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
            for (int x = 0; x < w; ++x) {
                const int src_x = std::clamp(x + sx, 0, w - 1);
                const int src_y = std::clamp(y + sy, 0, h - 1);
                dstRow[x] = src[static_cast<std::size_t>(src_y) * static_cast<std::size_t>(w) +
                                 static_cast<std::size_t>(src_x)];
            }
        });
    };

    const auto pixelCount = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    std::vector<float> scratch(pixelCount);
    shift_channel(scratch.data(), r_ch->data(), shiftR_x, shiftR_y);
    std::copy(scratch.begin(), scratch.end(), r_ch->data());
    shift_channel(scratch.data(), b_ch->data(), shiftB_x, shiftB_y);
    std::copy(scratch.begin(), scratch.end(), b_ch->data());
}

}
