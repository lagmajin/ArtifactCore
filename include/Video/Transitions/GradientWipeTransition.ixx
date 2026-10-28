module;

#include <cstdint>
#include <algorithm>
#include <cmath>

export module Video.Transitions.GradientWipeTransition;
import Video.VideoFrame;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class GradientWipeTransition final : public AbstractTransition {
public:
    const char* name() const override { return "GradientWipe"; }
    TransitionKind kind() const override { return TransitionKind::GradientWipe; }

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

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                // Gradient value: diagonal from top-left (0) to bottom-right (1)
                float g = static_cast<float>(x + y) / static_cast<float>(width + height - 1);
                // Soft step around a threshold that covers the whole gradient
                // range at both ends (p=0 keeps the left frame, p=1 the right).
                float soft = 0.1f;
                float threshold = -soft + p * (1.0f + soft * 2.0f);
                float t = std::clamp((threshold - g + soft) / (soft * 2.0f), 0.0f, 1.0f);

                for (int c = 0; c < 4; ++c) {
                    float l = leftScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                    float r = rightScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                    float v = l * (1.0f - t) + r * t;
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                        static_cast<std::uint8_t>(std::clamp(v + 0.5f, 0.0f, 255.0f));
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
void registerGradientWipeTransition();
}
