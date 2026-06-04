module;

#include <cstdint>
#include <algorithm>
#include <cmath>

export module Video.Transitions.SpinTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

namespace {

static constexpr double TwoPi = 6.28318530717958647692;

struct SpinSample {
    double u;
    double v;
    double weight;
};

static SpinSample sampleSpin(int x, int y, int width, int height, double progress)
{
    const double cx = width * 0.5;
    const double cy = height * 0.5;
    const double dx = x - cx;
    const double dy = y - cy;
    const double maxRadius = std::sqrt(cx * cx + cy * cy);
    const double radius = std::sqrt(dx * dx + dy * dy) / maxRadius;

    const double angle = std::atan2(dy, dx) + TwoPi * progress * radius * 0.5;
    const double srcR = radius * 2.0;
    const int srcX = static_cast<int>(cx + std::cos(angle - TwoPi * 0.5 * progress) * srcR * cx);
    const int srcY = static_cast<int>(cy + std::sin(angle - TwoPi * 0.5 * progress) * srcR * cy);

    return SpinSample{static_cast<double>(srcX), static_cast<double>(srcY), 1.0 - progress};
}

}

class SpinTransition final : public AbstractTransition {
public:
    const char* name() const override { return "Spin"; }
    TransitionKind kind() const override { return TransitionKind::Spin; }

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

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                const SpinSample ls = sampleSpin(x, y, width, height, ctx.progress);

                if (ls.weight > 0.5) {
                    const int lx = std::clamp(static_cast<int>(ls.u), 0, width - 1);
                    const int ly = std::clamp(static_cast<int>(ls.v), 0, height - 1);
                    for (int c = 0; c < 4; ++c) {
                        outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                            leftScan[static_cast<std::size_t>(ly) * static_cast<std::size_t>(left.strideBytes()) + static_cast<std::size_t>(lx) * 4u + static_cast<std::size_t>(c)];
                    }
                } else {
                    const SpinSample rs = sampleSpin(x, y, width, height, 1.0 - ctx.progress);
                    const int rx = std::clamp(static_cast<int>(rs.u), 0, width - 1);
                    const int ry = std::clamp(static_cast<int>(rs.v), 0, height - 1);
                    for (int c = 0; c < 4; ++c) {
                        outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                            rightScan[static_cast<std::size_t>(ry) * static_cast<std::size_t>(right.strideBytes()) + static_cast<std::size_t>(rx) * 4u + static_cast<std::size_t>(c)];
                    }
                }
            }
        }
    }
};

}
