module Graphics.Effect.Creative.Solarize;

import Graphics.Effect.Creative;
import Channel;
import std;

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
        for (int i = 0; i < w * h; ++i) {
            float v = data[i];
            if (v > th) {
                v = 1.0f - (v - th) / std::max(1e-5f, 1.0f - th);
            }
            data[i] = std::clamp(v, 0.0f, 1.0f);
        }
    };

    apply(r_ch->data());
    apply(g_ch->data());
    apply(b_ch->data());
}

}
