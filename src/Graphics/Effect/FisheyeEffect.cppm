module;
#include <vector>
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.Fisheye;

import Channel;

namespace ArtifactCore {

FisheyeEffect::FisheyeEffect() {
    parameters_.push_back({"Strength", "Curvature", EffectParameterType::Float, 0.5f, -1.0f, 1.0f});
    parameters_.push_back({"Zoom", "Zoom Level", EffectParameterType::Float, 1.0f, 0.5f, 2.0f});
}

void FisheyeEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);

    if (!r_ch || !g_ch || !b_ch) return;

    int w = frame.width();
    int h = frame.height();
    
    std::vector<float> r_old(r_ch->data(), r_ch->data() + r_ch->size());
    std::vector<float> g_old(g_ch->data(), g_ch->data() + g_ch->size());
    std::vector<float> b_old(b_ch->data(), b_ch->data() + b_ch->size());

    float cx = w * 0.5f;
    float cy = h * 0.5f;
    float str = strength();
    float z = zoom();
    
    float maxDist = std::sqrt(cx*cx + cy*cy);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = (x - cx) / maxDist;
            float dy = (y - cy) / maxDist;
            float rSq = dx*dx + dy*dy;
            
            // 樽型歪みモデル: r' = r * (1 + k * r^2)
            // ここでは簡易的に 1 + str * r^2 を使用
            float f = 1.0f + str * rSq;
            
            float sx = cx + (x - cx) * f / z;
            float sy = cy + (y - cy) * f / z;
            
            int isx = std::clamp((int)sx, 0, w - 1);
            int isy = std::clamp((int)sy, 0, h - 1);
            int idx = y * w + x;
            int sidx = isy * w + isx;
            
            r_ch->data()[idx] = r_old[sidx];
            g_ch->data()[idx] = g_old[sidx];
            b_ch->data()[idx] = b_old[sidx];
        }
    }
}

} // namespace ArtifactCore
