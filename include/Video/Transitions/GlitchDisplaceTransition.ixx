module;

#include <cstdint>
#include <algorithm>
#include <array>

export module Video.Transitions.GlitchDisplaceTransition;

import NLE.Core;
import Video.AbstractTransition;
import Video.CpuFrameView;

namespace ArtifactCore {

namespace {

struct GlitchRow {
    int y;
    int length;
    int offsetX;
};

static constexpr int GlitchRowProbabilityThreshold = 30;

static std::array<GlitchRow, 4> buildGlitchRows(int height, int width, double intensity)
{
    std::array<GlitchRow, 4> rows{};
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rows[i].y = static_cast<int>(static_cast<double>(height) * (0.2 + 0.6 * ((i + 1) / static_cast<double>(rows.size() + 1))));
        rows[i].length = static_cast<int>(static_cast<double>(height) * 0.08 * intensity);
        rows[i].offsetX = static_cast<int>((i % 2 == 0 ? 1 : -1) * intensity * static_cast<double>(width) * 0.12);
    }
    return rows;
}

static void copyRegion(const std::uint8_t* srcScan, std::uint8_t* dstScan,
                        int srcX, int dstX, int count, int width)
{
    if (srcX < 0 || dstX < 0 || srcX + count > width || dstX + count > width) {
        return;
    }
    std::memcpy(dstScan + static_cast<std::size_t>(dstX) * 4u,
                srcScan + static_cast<std::size_t>(srcX) * 4u,
                static_cast<std::size_t>(count) * 4u);
}

}

class GlitchDisplaceTransition final : public AbstractTransition {
public:
    const char* name() const override { return "GlitchDisplace"; }
    TransitionKind kind() const override { return TransitionKind::GlitchDisplace; }

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
        const float intensity = std::min(p * 2.0f, 1.0f);
        const float rgbShift = intensity * 6.0f;

        const std::array<GlitchRow, 4> rows = buildGlitchRows(height, width, intensity);

        for (int y = 0; y < height; ++y) {
            const std::uint8_t* leftScan = left.scanline(y);
            const std::uint8_t* rightScan = right.scanline(y);
            std::uint8_t* outScan = const_cast<std::uint8_t*>(leftScan);

            for (int x = 0; x < width; ++x) {
                const float weight = p;
                std::array<float, 4> out{};

                for (int c = 0; c < 4; ++c) {
                    const float base = leftScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] * (1.0f - weight);

                    int rx = std::clamp(x + static_cast<int>(rgbShift), 0, width - 1);
                    int bx = std::clamp(x - static_cast<int>(rgbShift), 0, width - 1);

                    const float rChannel = rightScan[static_cast<std::size_t>(rx) * 4u];
                    const float gChannel = leftScan[static_cast<std::size_t>(x) * 4u + 1u];
                    const float bChannel = rightScan[static_cast<std::size_t>(bx) * 4u + 2u];
                    const float aChannel = leftScan[static_cast<std::size_t>(x) * 4u + 3u];

                    switch (c) {
                    case 0: out[c] = base + weight * rChannel; break;
                    case 1: out[c] = base + weight * gChannel; break;
                    case 2: out[c] = base + weight * bChannel; break;
                    case 3: out[c] = aChannel; break;
                    }
                }

                for (int c = 0; c < 4; ++c) {
                    outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                        static_cast<std::uint8_t>(std::clamp(out[c] + 0.5f, 0.0f, 255.0f));
                }
            }

            for (const GlitchRow& row : rows) {
                if (std::abs(y - row.y) <= row.length / 2) {
                    const int offset = static_cast<int>(intensity * static_cast<float>(row.offsetX));
                    for (int x = 0; x < width; ++x) {
                        const int srcX = x - offset;
                        if (srcX >= 0 && srcX < width) {
                            for (int c = 0; c < 4; ++c) {
                                outScan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                                    rightScan[static_cast<std::size_t>(srcX) * 4u + static_cast<std::size_t>(c)];
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
};

}
