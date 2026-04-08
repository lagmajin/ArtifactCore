module;
#include <utility>
#include <vector>
#include <algorithm>

module Graphics.Effect.Creative.Pixelate;

import Channel;

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

    for (int by = 0; by < h; by += size) {
        for (int bx = 0; bx < w; bx += size) {
            
            // ブロック内の平均色を取得 (簡易化のため左上を採用してもいいが、平均の方が綺麗)
            float r_sum = 0, g_sum = 0, b_sum = 0;
            int count = 0;

            for (int y = by; y < by + size && y < h; ++y) {
                for (int x = bx; x < bx + size && x < w; ++x) {
                    int idx = y * w + x;
                    r_sum += r_ch->data()[idx];
                    g_sum += g_ch->data()[idx];
                    b_sum += b_ch->data()[idx];
                    count++;
                }
            }

            float r_avg = r_sum / count;
            float g_avg = g_sum / count;
            float b_avg = b_sum / count;

            // ブロック全体を平均色で塗りつぶす
            for (int y = by; y < by + size && y < h; ++y) {
                for (int x = bx; x < bx + size && x < w; ++x) {
                    int idx = y * w + x;
                    r_ch->data()[idx] = r_avg;
                    g_ch->data()[idx] = g_avg;
                    b_ch->data()[idx] = b_avg;
                }
            }
        }
    }
}

} // namespace ArtifactCore
