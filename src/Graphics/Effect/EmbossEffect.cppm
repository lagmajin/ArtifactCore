module Graphics.Effect.Creative.Emboss;

import Graphics.Effect.Creative;
import Channel;
import std;

namespace ArtifactCore {

EmbossEffect::EmbossEffect() {
    parameters_.push_back({"Strength", "Strength", EffectParameterType::Float, 1.0f, 0.0f, 5.0f});
    parameters_.push_back({"Height", "Height", EffectParameterType::Float, 1.0f, 0.0f, 10.0f});
}

void EmbossEffect::process(VideoFrame& frame, const CreativeEffectContext&) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);
    if (!r_ch || !g_ch || !b_ch) return;

    const int w = frame.width();
    const int h = frame.height();
    const float s = std::max(0.01f, strength());
    const float ht = std::max(0.0f, height());

    const int dx = static_cast<int>(std::round(s));
    const int dy = static_cast<int>(std::round(s));

    auto apply_emboss = [&](float* data) {
        std::vector<float> out(static_cast<size_t>(w) * static_cast<size_t>(h));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int ix = x + dx;
                const int iy = y + dy;
                const float cur = data[y * w + x];
                if (ix >= w || iy >= h) {
                    out[y * w + x] = cur;
                    continue;
                }
                float base = data[iy * w + ix];
                float diff = base - cur;
                if (ht > 0.0f) diff *= ht;
                out[y * w + x] = std::clamp(0.5f + diff, 0.0f, 1.0f);
            }
        }
        std::copy(out.begin(), out.end(), data);
    };

    apply_emboss(r_ch->data());
    apply_emboss(g_ch->data());
    apply_emboss(b_ch->data());
}

}
