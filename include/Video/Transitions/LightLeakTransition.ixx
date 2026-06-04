module;

#include <cstdint>
#include <algorithm>
#include <cmath>

export module Video.Transitions.LightLeakTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class LightLeakTransition final : public AbstractTransition {
public:
    const char* name() const override { return "LightLeak"; }
    TransitionKind kind() const override { return TransitionKind::LightLeak; }

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
        const float cy = height * 0.5f;
        const float p = static_cast<float>(ctx.progress);

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                float dx = static_cast<float>(x) - cx;
                float dy = static_cast<float>(y) - cy;
                float dist = std::sqrt(dx * dx + dy * dy);
                float maxDist = std::max(cx, cy);
                float mask = 1.0f - (dist / maxDist);
                mask = std::pow(std::clamp(mask, 0.0f, 1.0f), 2.0f);

                float leak = mask * p;
                float rightWeight = std::clamp(leak, 0.0f, 1.0f);
                float leftWeight = 1.0f - rightWeight;

                for (int c = 0; c < 3; ++c) {
                    float lv = leftScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                    float rv = rightScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)];
                    float blend = lv * leftWeight + rv * rightWeight;
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                        static_cast<std::uint8_t>(std::clamp(blend + 0.5f, 0.0f, 255.0f));
                }
                outScan[static_cast<std::size_t>(x) * 4u + 3u] = leftScan[static_cast<std::size_t>(x) * 4u + 3u];
            }
        }
    }
};

}
