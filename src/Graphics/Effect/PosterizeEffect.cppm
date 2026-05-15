module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.Posterize;

import Channel;

namespace ArtifactCore {

PosterizeEffect::PosterizeEffect() {
    parameters_.push_back({"Levels", "Color Levels", EffectParameterType::Float, 4.0f, 2.0f, 64.0f});
}

void PosterizeEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);

    if (!r_ch || !g_ch || !b_ch) return;

    int w = frame.width();
    int h = frame.height();
    
    float n = std::max(2.0f, levels());

    for (int i = 0; i < w * h; ++i) {
        // 階調を減らす (Quantization)
        // [0.0, 1.0] -> [0.0, n-1] -> floor -> [0.0, 1.0]
        r_ch->data()[i] = std::floor(r_ch->data()[i] * (n - 1.0f) + 0.5f) / (n - 1.0f);
        g_ch->data()[i] = std::floor(g_ch->data()[i] * (n - 1.0f) + 0.5f) / (n - 1.0f);
        b_ch->data()[i] = std::floor(b_ch->data()[i] * (n - 1.0f) + 0.5f) / (n - 1.0f);
    }
}

} // namespace ArtifactCore
