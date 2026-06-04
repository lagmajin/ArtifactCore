module;

#include <cstdint>

#include <QString>

export module Video.Transitions.CrossfadeTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

export namespace ArtifactCore {

class CrossfadeTransition final : public AbstractTransition {
public:
    const char* name() const override { return "Crossfade"; }
    TransitionKind kind() const override { return TransitionKind::Crossfade; }

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
        const float alpha = static_cast<float>(ctx.progress);

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < 4; ++c) {
                    const float lv = leftScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                    const float rv = rightScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                    const float blend = lv * (1.0f - alpha) + rv * alpha;
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(blend + 0.5f);
                }
            }
        }
    }
};

}
