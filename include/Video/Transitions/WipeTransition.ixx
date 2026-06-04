module;

#include <cstdint>

#include <QString>

export module Video.Transitions.WipeTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

export namespace ArtifactCore {

class WipeTransition final : public AbstractTransition {
public:
    const char* name() const override { return "Wipe"; }
    TransitionKind kind() const override { return TransitionKind::Wipe; }

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
        const int threshold = static_cast<int>(ctx.progress * static_cast<double>(width));

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                const bool useRight = x >= threshold;
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
