module;
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

module Graphics.Effect.Creative.OldTV;

import Channel;
import Math.Noise;

namespace ArtifactCore {

OldTVEffect::OldTVEffect() {
    parameters_.push_back({"Scanline", "Scanline Density", EffectParameterType::Float, 0.5f, 0.0f, 1.0f});
    parameters_.push_back({"Curvature", "Screen Curvature", EffectParameterType::Float, 0.2f, 0.0f, 0.5f});
    parameters_.push_back({"Flicker", "Flicker Noise", EffectParameterType::Float, 0.1f, 0.0f, 0.5f});
    parameters_.push_back({"Fringe", "Color Fringe", EffectParameterType::Float, 0.3f, 0.0f, 1.0f});
}

void OldTVEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);

    if (!r_ch || !g_ch || !b_ch) return;

    int w = frame.width();
    int h = frame.height();
    float time = (float)context.time;
    
    std::vector<float> r_old(r_ch->data(), r_ch->data() + r_ch->size());
    std::vector<float> g_old(g_ch->data(), g_ch->data() + g_ch->size());
    std::vector<float> b_old(b_ch->data(), b_ch->data() + b_ch->size());

    float curve = curvature();
    float flic = flicker();
    float fringe = chromaticAberration();
    float slDens = scanlineDensity();
    
    float cx = w * 0.5f;
    float cy = h * 0.5f;
    float maxDist = std::sqrt(cx*cx + cy*cy);

    for (int y = 0; y < h; ++y) {
        // 全体のちらつき (Flicker)
        float globalNoise = (NoiseGenerator::perlin(time * 50.0f) - 0.5f) * flic;
        
        // 特定の走査線のズレ (水平方向のランダムな揺れ)
        float rowJitter = (NoiseGenerator::perlin(y * 0.5f, time * 10.0f) - 0.5f) * 2.0f * flic;

        for (int x = 0; x < w; ++x) {
            // 1. CRT Curvature (バレル歪み)
            float dx = (x - cx) / maxDist;
            float dy = (y - cy) / maxDist;
            float rSq = dx*dx + dy*dy;
            float distCorrection = 1.0f + curve * rSq;
            
            float sx = cx + (x - cx + rowJitter) * distCorrection;
            float sy = cy + (y - cy) * distCorrection;

            // 各チャンネルのずらし幅 (Chromatic Aberration)
            // 外側に行くほどずれが強くなる、レンズ特有の色分散
            float splitX = fringe * 5.0f * (std::abs(x-cx) / cx);
            
            // 2. Sampling (Rは左、Bは右に)
            auto getVal = [&](const std::vector<float>& src, float fx, float fy) {
                int isx = std::clamp((int)fx, 0, w - 1);
                int isy = std::clamp((int)fy, 0, h - 1);
                return src[isy * w + isx];
            };

            float finalR = getVal(r_old, sx - splitX, sy);
            float finalG = getVal(g_old, sx, sy);
            float finalB = getVal(b_old, sx + splitX, sy);

            // 3. Scanline (走査線)
            // 明るさをsin波で周期的に落とす
            float sl = 1.0f - std::abs(std::sin(sy * slDens * 2.0f)) * 0.2f;
            
            // 4. White Noise (Grain)
            float grain = (NoiseGenerator::perlin(x * 10.0f, y * 10.0f, time * 60.0f) - 0.5f) * 0.05f * flic;

            int idx = y * w + x;
            r_ch->data()[idx] = (finalR + globalNoise + grain) * sl;
            g_ch->data()[idx] = (finalG + globalNoise + grain) * sl;
            b_ch->data()[idx] = (finalB + globalNoise + grain) * sl;
        }
    }
}

} // namespace ArtifactCore
