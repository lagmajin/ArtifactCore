module;

#include <cstdint>
#include <algorithm>
#include <cmath>

export module Video.Transitions.IrisWipeTransition;
import Video.VideoFrame;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class IrisWipeTransition final : public AbstractTransition {
public:
    const char* name() const override { return "IrisWipe"; }
    TransitionKind kind() const override { return TransitionKind::IrisWipe; }

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

        const float cx = static_cast<float>(width) * 0.5f;
        const float cy = static_cast<float>(height) * 0.5f;
        const float maxR = std::sqrt(cx * cx + cy * cy);

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                float dx = static_cast<float>(x) - cx;
                float dy = static_cast<float>(y) - cy;
                float dist = std::sqrt(dx * dx + dy * dy);
                float t = dist / maxR;
                // Threshold covers t in [0, 1] at both ends: p=0 keeps the left
                // frame, p=1 reveals the right frame everywhere (including the
                // soft centre disc).
                float edge = (1.0f - p) * 1.05f - 0.05f;
                float weight = std::clamp((t - edge) / 0.05f, 0.0f, 1.0f);

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

// Explicit registration hook: a static-library consumer cannot
// rely on the initializer of this implementation unit to run, so a
// host may register the creator deterministically instead.
export namespace ArtifactCore {
void registerIrisWipeTransition();
}
