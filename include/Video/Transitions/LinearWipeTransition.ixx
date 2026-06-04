module;

#include <cstdint>
#include <algorithm>

export module Video.Transitions.LinearWipeTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class LinearWipeTransition final : public AbstractTransition {
public:
    const char* name() const override { return "LinearWipe"; }
    TransitionKind kind() const override { return TransitionKind::LinearWipe; }

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

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                const float t = static_cast<float>(x + y) / static_cast<float>(width + height - 2);
                const bool useRight = t >= static_cast<float>(ctx.progress);
                const std::uint8_t* src = useRight ? rightScan : leftScan;
                for (int c = 0; c < 4; ++c) {
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] = src[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                }
            }
        }
    }
};

}
