module;
#include <utility>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>

module Graphics.Effect.Creative.Glitch;

import Channel;
import Core.Parallel;
import Math.Noise;

namespace ArtifactCore {

GlitchCreativeEffect::GlitchCreativeEffect() {
    parameters_.push_back({"Amount", "Intensity", EffectParameterType::Float, 0.5f, 0.0f, 1.0f});
    parameters_.push_back({"RGBSplit", "RGB Split", EffectParameterType::Float, 0.3f, 0.0f, 1.0f});
    parameters_.push_back({"BlockNoise", "Block Noise", EffectParameterType::Float, 0.2f, 0.0f, 1.0f});
}

void GlitchCreativeEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    if (!enabled_) return;

    auto r = frame.getChannel(ChannelType::Red);
    auto g = frame.getChannel(ChannelType::Green);
    auto b = frame.getChannel(ChannelType::Blue);
    auto a = frame.getChannel(ChannelType::Alpha);

    if (!r || !g || !b) return;

    int w = frame.width();
    int h = frame.height();
    float time = (float)context.time;

    // 1. ブロッキーなズレ (Displacement) 
    // ※ 簡易化のため別のバッファを作成（実際には最適化が必要）
    std::vector<float> r_old = std::vector<float>(r->data(), r->data() + r->size());
    std::vector<float> g_old = std::vector<float>(g->data(), g->data() + g->size());
    std::vector<float> b_old = std::vector<float>(b->data(), b->data() + b->size());

    float amount = glitchAmount();
    float split = rgbSplit();
    float* rData = r->data();
    float* gData = g->data();
    float* bData = b->data();

    (void)NoiseGenerator::perlin(0.0f, 0.0f, 0.0f);
    Parallel::For(0, h, w * h, [&](int y) {
        float* rRow = rData + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        float* gRow = gData + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        float* bRow = bData + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        const std::size_t sourceRow = static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        // 水平ラインごとのズレ (グリッチ特有の走査線ズレ)
        float lineNoise = NoiseGenerator::perlin(y * 0.1f, time * 20.0f);
        float xOffset = 0.0f;
        if (std::abs(lineNoise) > 0.6f) { // 時々大きくズレる
             xOffset = lineNoise * 50.0f * amount;
        }

        for (int x = 0; x < w; ++x) {
            int srcX = (int)(x + xOffset);
            srcX = std::clamp(srcX, 0, w - 1);
            
            // 2. RGB 分離 (Chromatic Aberration)
            // R は少し左に、B は少し右にさらにズらす
            int rx = std::clamp((int)(srcX - split * 15.0f * amount), 0, w - 1);
            int gx = srcX;
            int bx = std::clamp((int)(srcX + split * 15.0f * amount), 0, w - 1);

            rRow[x] = r_old[sourceRow + static_cast<std::size_t>(rx)];
            gRow[x] = g_old[sourceRow + static_cast<std::size_t>(gx)];
            bRow[x] = b_old[sourceRow + static_cast<std::size_t>(bx)];

            // 3. 粒子のノイズ (Grain) をほんの少し載せる
            float grain = (NoiseGenerator::perlin(x * 0.5f, y * 0.5f, time * 50.0f) - 0.5f) * 0.05f;
            rRow[x] += grain;
            gRow[x] += grain;
            bRow[x] += grain;
        }
    });
}

} // namespace ArtifactCore
