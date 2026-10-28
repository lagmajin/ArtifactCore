module;

#include <cstdint>
#include <algorithm>
#include <cmath>

export module Video.Transitions.DoorsTransition;
import Video.VideoFrame;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class DoorsTransition final : public AbstractTransition {
public:
    const char* name() const override { return "Doors"; }
    TransitionKind kind() const override { return TransitionKind::Doors; }

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
        const float cx = width * 0.5f;
        const float p = static_cast<float>(ctx.progress);

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                // Normalised distance from the centre: 0 in the middle, 1 at the
                // side edges. Progress opens the incoming frame outward, so p=0
                // is the left frame and p=1 the right frame.
                const float d = std::fabs((static_cast<float>(x) + 0.5f) - cx) / cx;
                const bool useRight = d < p;
                const std::uint8_t* src = useRight ? rightScan : leftScan;
                for (int c = 0; c < 4; ++c) {
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                        src[static_cast<std::size_t>(std::clamp(x, 0, width - 1)) * 4u + static_cast<std::size_t>(c)];
                }
            }
        }
    }
};

}

// Explicit registration hook: a static-library consumer cannot
// rely on the initializer of this implementation unit to run, so a
// host may register the creator deterministically instead.
export namespace ArtifactCore {
void registerDoorsTransition();
}
