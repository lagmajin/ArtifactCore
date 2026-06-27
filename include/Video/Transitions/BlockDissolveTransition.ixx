module;

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <random>

export module Video.Transitions.BlockDissolveTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class BlockDissolveTransition final : public AbstractTransition {
public:
    const char* name() const override { return "BlockDissolve"; }
    TransitionKind kind() const override { return TransitionKind::BlockDissolve; }

    void process(const DecodedVideoFrame& leftFrame,
                 const DecodedVideoFrame& rightFrame,
                 const TransitionContext& ctx) override
    {
        const CpuFrameView left(leftFrame);
        const CpuFrameView right(rightFrame);
        if (!left.isValid() || !right.isValid()) return;

        const int width = std::min(left.width(), right.width());
        const int height = std::min(left.height(), right.height());
        const float p = static_cast<float>(ctx.progress);

        const int blockSize = 8;
        const int cols = (width + blockSize - 1) / blockSize;
        const int rows = (height + blockSize - 1) / blockSize;

        // Deterministic random threshold per block using hash
        auto blockThreshold = [](int bx, int by) -> float {
            float h = std::sin(static_cast<float>(bx) * 127.1f + static_cast<float>(by) * 311.7f) * 43758.5453f;
            h = h - std::floor(h);
            return h;
        };

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            int by = y / blockSize;

            for (int x = 0; x < width; ++x) {
                int bx = x / blockSize;
                float threshold = blockThreshold(bx, by);
                float weight = (threshold < p) ? 1.0f : 0.0f;

                for (int c = 0; c < 4; ++c) {
                    float l = leftScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                    float r = rightScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                    float v = l * (1.0f - weight) + r * weight;
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                        static_cast<std::uint8_t>(std::clamp(v + 0.5f, 0.0f, 255.0f));
                }
            }
        }
    }
};

}
