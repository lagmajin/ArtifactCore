module;

#include <cstdint>
#include <algorithm>
#include <cmath>

export module Video.Transitions.CubeTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class CubeTransition final : public AbstractTransition {
public:
    const char* name() const override { return "Cube"; }
    TransitionKind kind() const override { return TransitionKind::Cube; }

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
        const float p = static_cast<float>(ctx.progress);

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                const float u = static_cast<float>(x) / static_cast<float>(width);
                const float v = static_cast<float>(y) / static_cast<float>(height);

                float cubeU = (u - 0.5f) / std::max(0.0001f, 1.0f - p * 0.5f) + 0.5f;
                float cubeV = v;

                bool useRight = cubeU < 0.0f || cubeU > 1.0f;
                cubeU = std::clamp(cubeU, 0.0f, 1.0f);

                const int srcX = static_cast<int>(cubeU * static_cast<float>(width));
                const int readX = std::clamp(srcX, 0, width - 1);
                const std::uint8_t* src = useRight ? rightScan : leftScan;

                for (int c = 0; c < 4; ++c) {
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                        src[static_cast<std::size_t>(readX) * 4u + static_cast<std::size_t>(c)];
                }
            }
        }
    }
};

}
