module;

#include <cstdint>
#include <algorithm>

#include <QColor>

export module Video.Transitions.ZoomTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

export namespace ArtifactCore {

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
        const int cx = width / 2;
        const int cy = height / 2;
        const double p = ctx.progress;

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                const double u = static_cast<double>(x - cx) / static_cast<double>(cx);
                const double v = static_cast<double>(y - cy) / static_cast<double>(cy);

                double leftU = u / (0.5 + 0.5 * p);
                double leftV = v / (0.5 + 0.5 * p);
                int lx = cx + static_cast<int>(leftU * cx);
                int ly = cy + static_cast<int>(leftV * cy);
                lx = std::clamp(lx, 0, width - 1);
                ly = std::clamp(ly, 0, height - 1);

                double rightU = u / (1.0 - 0.5 * p);
                double rightV = v / (1.0 - 0.5 * p);
                int rx = cx + static_cast<int>(rightU * cx);
                int ry = cy + static_cast<int>(rightV * cy);
                rx = std::clamp(rx, 0, width - 1);
                ry = std::clamp(ry, 0, height - 1);

                const float leftW = static_cast<float>(1.0 - p);
                const float rightW = static_cast<float>(p);

                for (int c = 0; c < 4; ++c) {
                    const float lv = leftScan[static_cast<std::size_t>(lx) * 4u + static_cast<std::size_t>(c)];
                    const float rv = rightScan[static_cast<std::size_t>(rx) * 4u + static_cast<std::size_t>(c)];
                    const float blend = lv * leftW + rv * rightW;
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(blend + 0.5f);
                }
            }
        }
    }
};

}
