module;

#include <cstdint>
#include <algorithm>

export module Video.Transitions.DissolveTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class DissolveTransition final : public AbstractTransition {
public:
    const char* name() const override { return "Dissolve"; }
    TransitionKind kind() const override { return TransitionKind::Dissolve; }

    void process(const DecodedVideoFrame& leftFrame,
                 const DecodedVideoFrame& rightFrame,
                 const TransitionContext& ctx) override
    {
        const CpuFrameView left(leftFrame);
        const CpuFrameView right(rightFrame);
        if (!left.isValid() || !right.isValid()) {
            return;
        }

        const int width = std::min(left.width(), right.width());
        const int height = std::min(left.height(), right.height());
        const float threshold = static_cast<float>(ctx.progress) * 255.0f;

        const std::uint32_t seed = static_cast<std::uint32_t>(ctx.frameIndex) + static_cast<std::uint32_t>(ctx.fps);
        auto hash = [seed](int x, int y) {
            std::uint32_t h = seed + static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u;
            h = (h ^ (h >> 13u)) * 1274126177u;
            return static_cast<float>(h & 0xffu);
        };

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                const float noise = hash(x, y);
                const bool useRight = noise <= threshold;
                const std::uint8_t* src = useRight ? rightScan : leftScan;
                for (int c = 0; c < 4; ++c) {
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                        src[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                }
            }
        }
    }
};

}
