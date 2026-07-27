module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.Posterize;

import Channel;
import Core.Parallel;

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
    float* rData = r_ch->data();
    float* gData = g_ch->data();
    float* bData = b_ch->data();

    Parallel::For(0, w * h, w * h, [&](int i) {
        // 階調を減らす (Quantization)
        // [0.0, 1.0] -> [0.0, n-1] -> floor -> [0.0, 1.0]
        rData[i] = std::floor(rData[i] * (n - 1.0f) + 0.5f) / (n - 1.0f);
        gData[i] = std::floor(gData[i] * (n - 1.0f) + 0.5f) / (n - 1.0f);
        bData[i] = std::floor(bData[i] * (n - 1.0f) + 0.5f) / (n - 1.0f);
    });
}

} // namespace ArtifactCore
