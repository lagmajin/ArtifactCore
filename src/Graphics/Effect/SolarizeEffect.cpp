module;

#include <algorithm>

module Graphics.Effect.Creative.Solarize;

import Graphics.Effect.Creative;
import Channel;
import Core.Parallel;
namespace ArtifactCore {

SolarizeEffect::SolarizeEffect() {
    parameters_.push_back({"Threshold", "Threshold", EffectParameterType::Float, 0.5f, 0.0f, 1.0f});
}

void SolarizeEffect::process(VideoFrame& frame, const CreativeEffectContext&) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);
    if (!r_ch || !g_ch || !b_ch) return;

    const int w = frame.width();
    const int h = frame.height();
    const float th = std::clamp(threshold(), 0.0f, 1.0f);

    auto apply = [&](float* data) {
        Parallel::ForPixels(0, h, w, h, [&](int y) {
            const std::size_t rowStart = static_cast<std::size_t>(y) *
                                         static_cast<std::size_t>(w);
            for (int x = 0; x < w; ++x) {
                const std::size_t index = rowStart + static_cast<std::size_t>(x);
                float v = data[index];
                if (v > th) {
                    v = 1.0f - (v - th) / std::max(1e-5f, 1.0f - th);
                }
                data[index] = std::clamp(v, 0.0f, 1.0f);
            }
        });
    };

    apply(r_ch->data());
    apply(g_ch->data());
    apply(b_ch->data());
}

}
