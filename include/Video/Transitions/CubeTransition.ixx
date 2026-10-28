module;

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <vector>

export module Video.Transitions.CubeTransition;
import Video.VideoFrame;

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
        // A occupies [0, span) and is squeezed away as the cube turns; B fills
        // [span, 1) and expands to the whole frame at p=1.
        const float span = 1.0f - p;
        const float epsilon = 0.0001f;

        // process() writes into the left frame, so the A face is sampled from an
        // immutable copy. The right frame is only read.
        const std::size_t stride = static_cast<std::size_t>(left.strideBytes());
        const std::vector<std::uint8_t> source(
            left.data(), left.data() + stride * static_cast<std::size_t>(height));

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan =
                source.data() + static_cast<std::size_t>(y) * stride;
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(left.scanline(y));

            for (int x = 0; x < width; ++x) {
                const float u =
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
                const bool useRight = u >= span;
                const float local = useRight
                    ? (u - span) / std::max(epsilon, p)
                    : u / std::max(epsilon, span);
                const int readX = std::clamp(
                    static_cast<int>(local * static_cast<float>(width)), 0,
                    width - 1);
                const std::uint8_t* src = useRight ? rightScan : leftScan;

                for (int c = 0; c < 4; ++c) {
                    outScan[static_cast<std::size_t>(x) * 4u +
                            static_cast<std::size_t>(c)] =
                        src[static_cast<std::size_t>(readX) * 4u +
                            static_cast<std::size_t>(c)];
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
void registerCubeTransition();
}
