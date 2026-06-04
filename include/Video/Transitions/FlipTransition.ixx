module;

#include <cstdint>
#include <algorithm>

export module Video.Transitions.FlipTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class FlipTransition final : public AbstractTransition {
public:
    const char* name() const override { return "Flip"; }
    TransitionKind kind() const override { return TransitionKind::Flip; }

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

            const float t = static_cast<float>(ctx.progress);
            for (int x = 0; x < width; ++x) {
                const float u = static_cast<float>(x) / static_cast<float>(width);
                const bool flipped = u >= t;
                const int mirrorX = static_cast<int>((1.0f - u) * static_cast<float>(width));
                const int clampedMirrorX = std::clamp(mirrorX, 0, width - 1);
                const std::uint8_t* srcScan = flipped ? rightScan : leftScan;
                const std::uint8_t* refScan = flipped ? leftScan : rightScan;
                const int readX = flipped ? std::clamp(x, 0, width - 1) : clampedMirrorX;
                const int altX = flipped ? x : clampedMirrorX;

                for (int c = 0; c < 4; ++c) {
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                        srcScan[static_cast<std::size_t>(readX) * 4u + static_cast<std::size_t>(c)];
                }
            }
        }
    }
};

}
