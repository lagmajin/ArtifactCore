module;

#include <cstdint>
#include <algorithm>

#include <QColor>

export module Video.Transitions.SlideTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

export namespace ArtifactCore {

class SlideTransition final : public AbstractTransition {
public:
    const char* name() const override { return "Slide"; }
    TransitionKind kind() const override { return TransitionKind::Slide; }

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
        const double p = ctx.progress;

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            const int rightShift = static_cast<int>(p * static_cast<double>(width));

            for (int x = 0; x < width; ++x) {
                const bool useLeft = x >= rightShift;
                const std::uint8_t* src = useLeft ? leftScan : rightScan;
                const int srcX = useLeft ? x : (x - rightShift + width) % width;
                for (int c = 0; c < 4; ++c) {
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                        src[static_cast<std::size_t>(srcX) * 4u + static_cast<std::size_t>(c)];
                }
            }
        }
    }
};

}
