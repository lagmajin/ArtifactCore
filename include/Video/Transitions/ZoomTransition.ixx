module;

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <vector>

export module Video.Transitions.ZoomTransition;
import Video.VideoFrame;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

class ZoomTransition final : public AbstractTransition {
public:
    const char* name() const override { return "Zoom"; }
    TransitionKind kind() const override { return TransitionKind::Zoom; }

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
        const float cx = static_cast<float>(width) * 0.5f;
        const float cy = static_cast<float>(height) * 0.5f;
        const float p = static_cast<float>(ctx.progress);
        // The outgoing frame zooms in while it fades out; the incoming frame
        // starts zoomed in and settles to a 1:1 mapping at p=1.
        const float leftScale = 1.0f + p;
        const float rightScale = 2.0f - p;
        const float leftWeight = 1.0f - p;
        const float rightWeight = p;

        // process() writes into the left frame, so the outgoing frame is
        // sampled from an immutable copy. The right frame is only read.
        const std::size_t stride = static_cast<std::size_t>(left.strideBytes());
        const std::vector<std::uint8_t> source(
            left.data(), left.data() + stride * static_cast<std::size_t>(height));

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(left.scanline(y));

            for (int x = 0; x < width; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) - cx;
                const float dy = (static_cast<float>(y) + 0.5f) - cy;

                const int lx = std::clamp(
                    static_cast<int>(cx + dx / leftScale), 0, width - 1);
                const int ly = std::clamp(
                    static_cast<int>(cy + dy / leftScale), 0, height - 1);
                const int rx = std::clamp(
                    static_cast<int>(cx + dx / rightScale), 0, width - 1);
                const int ry = std::clamp(
                    static_cast<int>(cy + dy / rightScale), 0, height - 1);

                const std::uint8_t* leftPixel =
                    source.data() + static_cast<std::size_t>(ly) * stride +
                    static_cast<std::size_t>(lx) * 4u;
                const std::uint8_t* rightPixel =
                    right.data() + static_cast<std::size_t>(ry) *
                                       static_cast<std::size_t>(right.strideBytes()) +
                    static_cast<std::size_t>(rx) * 4u;

                for (int c = 0; c < 4; ++c) {
                    const float blend = static_cast<float>(leftPixel[c]) * leftWeight +
                                        static_cast<float>(rightPixel[c]) * rightWeight;
                    outScan[static_cast<std::size_t>(x) * 4u +
                            static_cast<std::size_t>(c)] =
                        static_cast<std::uint8_t>(blend + 0.5f);
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
void registerZoomTransition();
}
