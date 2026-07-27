module;
#include <utility>
#include <vector>
#include <algorithm>

module Graphics.Effect.Creative.Pixelate;

import Channel;
import Core.Parallel;

namespace ArtifactCore {

PixelateEffect::PixelateEffect() {
    parameters_.push_back({"BlockSize", "Block Size", EffectParameterType::Float, 10.0f, 1.0f, 100.0f});
}

void PixelateEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);

    if (!r_ch || !g_ch || !b_ch) return;

    int w = frame.width();
    int h = frame.height();
    
    int size = static_cast<int>(std::max(1.0f, blockSize()));
    float* rData = r_ch->data();
    float* gData = g_ch->data();
    float* bData = b_ch->data();

    const int blockRows = (h + size - 1) / size;
    Parallel::For(0, blockRows, w * h, [&](int blockY) {
        const int by = blockY * size;
        for (int bx = 0; bx < w; bx += size) {
            
            // ブロック内の平均色を取得 (簡易化のため左上を採用してもいいが、平均の方が綺麗)
            float r_sum = 0, g_sum = 0, b_sum = 0;
            int count = 0;

            for (int y = by; y < by + size && y < h; ++y) {
                const std::size_t rowBase = static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
                for (int x = bx; x < bx + size && x < w; ++x) {
                    const std::size_t idx = rowBase + static_cast<std::size_t>(x);
                    r_sum += rData[idx];
                    g_sum += gData[idx];
                    b_sum += bData[idx];
                    count++;
                }
            }

            float r_avg = r_sum / count;
            float g_avg = g_sum / count;
            float b_avg = b_sum / count;

            // ブロック全体を平均色で塗りつぶす
            for (int y = by; y < by + size && y < h; ++y) {
                const std::size_t rowBase = static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
                for (int x = bx; x < bx + size && x < w; ++x) {
                    const std::size_t idx = rowBase + static_cast<std::size_t>(x);
                    rData[idx] = r_avg;
                    gData[idx] = g_avg;
                    bData[idx] = b_avg;
                }
            }
        }
    });
}

} // namespace ArtifactCore
