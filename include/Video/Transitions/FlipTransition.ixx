module;

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <vector>

export module Video.Transitions.FlipTransition;
import Video.VideoFrame;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

namespace {

constexpr float kPi = 3.14159265358979323846f;

}

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
        const float p = static_cast<float>(ctx.progress);

        // A card turn around the vertical axis: the front face (left frame)
        // shrinks to the edge at p=0.5, then the back face (right frame) grows
        // back to full width at p=1.
        const float cosine = std::cos(kPi * p);
        const bool frontFace = cosine >= 0.0f;
        const float scale = std::max(0.02f, std::fabs(cosine));
        const float cx = static_cast<float>(width) * 0.5f;

        // process() writes into the left frame, so the front face is sampled
        // from an immutable copy. The right frame is only read.
        const std::size_t stride = static_cast<std::size_t>(left.strideBytes());
        const std::vector<std::uint8_t> source(
            left.data(), left.data() + stride * static_cast<std::size_t>(height));

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan =
                source.data() + static_cast<std::size_t>(y) * stride;
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(left.scanline(y));

            for (int x = 0; x < width; ++x) {
                const float srcXf =
                    cx + ((static_cast<float>(x) + 0.5f) - cx) / scale;
                const int readX =
                    std::clamp(static_cast<int>(srcXf), 0, width - 1);
                const std::uint8_t* src = frontFace ? leftScan : rightScan;

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
void registerFlipTransition();
}
