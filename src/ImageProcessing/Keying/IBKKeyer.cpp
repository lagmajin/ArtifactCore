module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

module ImageProcessing;
import :IBKKeyer;
import Core.Parallel;
import Graphics.SurfaceColorContract;

namespace ArtifactCore::Keying {
namespace {

float finiteOr(float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
}

float smoothstep(float edge0, float edge1, float value) {
    if (edge1 <= edge0) return value > edge0 ? 1.0f : 0.0f;
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void morphology(std::vector<float>& matte, int width, int height,
                int radius, bool erode)
{
    if (radius <= 0) return;
    std::vector<float> result(matte.size(), erode ? 1.0f : 0.0f);
    Parallel::ForTiles(width, height, 16, 16, [&](int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float value = erode ? 1.0f : 0.0f;
            const int minY = std::max(0, y - radius);
            const int maxY = std::min(height - 1, y + radius);
            const int minX = std::max(0, x - radius);
            const int maxX = std::min(width - 1, x + radius);
            for (int sy = minY; sy <= maxY; ++sy) {
                for (int sx = minX; sx <= maxX; ++sx) {
                    const float sample = matte[static_cast<std::size_t>(sy) * width + sx];
                    value = erode ? std::min(value, sample) : std::max(value, sample);
                }
            }
            result[static_cast<std::size_t>(y) * width + x] = value;
        }
    }
    });
    matte.swap(result);
}

} // namespace

bool processIBK(const IBKBuffers& buffers, const IBKParams& inputParams) {
    if (!buffers.foreground || !buffers.cleanPlate || !buffers.outputRGBA ||
        buffers.width <= 0 || buffers.height <= 0) {
        return false;
    }
    if ((buffers.foregroundOrder != SurfaceChannelOrder::RGBA &&
         buffers.foregroundOrder != SurfaceChannelOrder::BGRA) ||
        (buffers.cleanPlateOrder != SurfaceChannelOrder::RGBA &&
         buffers.cleanPlateOrder != SurfaceChannelOrder::BGRA))
        return false;
    const int fgR = buffers.foregroundOrder == SurfaceChannelOrder::RGBA ? 0 : 2;
    constexpr int fgG = 1;
    const int fgB = buffers.foregroundOrder == SurfaceChannelOrder::RGBA ? 2 : 0;
    const int plateR = buffers.cleanPlateOrder == SurfaceChannelOrder::RGBA ? 0 : 2;
    constexpr int plateG = 1;
    const int plateB = buffers.cleanPlateOrder == SurfaceChannelOrder::RGBA ? 2 : 0;
    const auto width = static_cast<std::size_t>(buffers.width);
    const auto height = static_cast<std::size_t>(buffers.height);
    if (height != 0 && width > std::numeric_limits<std::size_t>::max() / height)
        return false;
    const std::size_t pixelCount = width * height;
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4) return false;

    const float screenCorrection = std::max(0.0f, finiteOr(inputParams.screenCorrection, 1.0f));
    const float coreClip = std::clamp(finiteOr(inputParams.coreMatteClip, 0.5f), 0.0f, 1.0f);
    const float edgeSoftness = std::max(1.0e-5f, finiteOr(inputParams.edgeMatteSoftness, 0.2f));
    const float despill = std::clamp(finiteOr(inputParams.despillStrength, 0.5f), 0.0f, 1.0f);
    const float gamma = std::max(1.0e-5f, finiteOr(inputParams.garbageMatteGamma, 1.0f));
    const float detail = std::clamp(finiteOr(inputParams.detailRecovery, 0.3f), 0.0f, 1.0f);
    std::vector<float> matte(pixelCount, 0.0f);

    Parallel::ForTiles(buffers.width, buffers.height, 32, 32, [&](int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
        const std::size_t i = static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x);
        const float* fg = buffers.foreground + i * 4;
        const float* plate = buffers.cleanPlate + i * 4;
        const float r = std::max(0.0f, fg[fgR] * screenCorrection);
        const float g = std::max(0.0f, fg[fgG] * screenCorrection);
        const float b = std::max(0.0f, fg[fgB] * screenCorrection);
        const float dr = r - plate[plateR];
        const float dg = g - plate[plateG];
        const float db = b - plate[plateB];
        const float distance = std::sqrt(dr * dr + dg * dg + db * db);
        const float raw = 1.0f - std::exp(-distance / 0.25f);
        const float core = std::clamp(raw - coreClip, 0.0f, 1.0f);
        const float edge = smoothstep(0.0f, edgeSoftness, raw * (1.0f - core));
        matte[i] = std::pow(std::clamp(core + edge * detail, 0.0f, 1.0f), gamma);
        }
    }
    });

    morphology(matte, buffers.width, buffers.height,
               std::clamp(inputParams.erodePixels, 0, 64), true);
    morphology(matte, buffers.width, buffers.height,
               std::clamp(inputParams.dilatePixels, 0, 64), false);

    Parallel::ForTiles(buffers.width, buffers.height, 32, 32, [&](int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
        const std::size_t i = static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x);
        const float* fg = buffers.foreground + i * 4;
        const float* plate = buffers.cleanPlate + i * 4;
        float* out = buffers.outputRGBA + i * 4;
        const float alpha = std::clamp(matte[i] * std::clamp(fg[3], 0.0f, 1.0f), 0.0f, 1.0f);
        const float correctedR = std::max(0.0f, fg[fgR] * screenCorrection);
        const float correctedG = std::max(0.0f, fg[fgG] * screenCorrection);
        const float correctedB = std::max(0.0f, fg[fgB] * screenCorrection);
        out[fgR] = std::max(0.0f, correctedR - plate[plateR] * despill) * alpha;
        out[fgG] = std::max(0.0f, correctedG - plate[plateG] * despill) * alpha;
        out[fgB] = std::max(0.0f, correctedB - plate[plateB] * despill) * alpha;
        out[3] = alpha;
        }
    }
    });
    return true;
}

bool autoGenerateCleanPlate(const std::vector<ImageF32x4_RGBA>& frames,
                            ImageF32x4_RGBA& outCleanPlate) {
    if (frames.empty() || frames.size() > 4096) return false;
    const int width = frames.front().width();
    const int height = frames.front().height();
    if (width <= 0 || height <= 0) return false;
    const std::size_t pixelCount = static_cast<std::size_t>(width) *
                                   static_cast<std::size_t>(height);
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4) return false;
    for (const auto& frame : frames) {
        if (frame.width() != width || frame.height() != height ||
            !frame.rgba32fData()) return false;
        const auto order = frame.colorDescriptor().channelOrder;
        if (order != SurfaceChannelOrder::RGBA && order != SurfaceChannelOrder::BGRA)
            return false;
    }

    std::vector<float> result(pixelCount * 4, 0.0f);
    Parallel::ForTiles(width, height, 16, 16, [&](int x0, int y0, int x1, int y1) {
    std::vector<float> values;
    values.reserve(frames.size());
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
        const std::size_t pixel = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
        for (int channel = 0; channel < 4; ++channel) {
            values.clear();
            for (const auto& frame : frames) {
                const auto order = frame.colorDescriptor().channelOrder;
                const int sourceChannel = channel == 3 || channel == 1
                    ? channel
                    : (order == SurfaceChannelOrder::RGBA
                        ? channel : 2 - channel);
                const float sample = frame.rgba32fData()[pixel * 4 + sourceChannel];
                values.push_back(std::isfinite(sample) ? sample : 0.0f);
            }
            const auto middle = values.begin() + values.size() / 2;
            std::nth_element(values.begin(), middle, values.end());
            if (values.size() % 2 == 0) {
                const float upper = *middle;
                const auto lower = std::max_element(values.begin(), middle);
                result[pixel * 4 + channel] = (*lower + upper) * 0.5f;
            } else {
                result[pixel * 4 + channel] = *middle;
            }
        }
    }
    }
    });
    outCleanPlate.setFromRGBA32F(result.data(), width, height);
    auto cleanPlateDescriptor =
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied();
    cleanPlateDescriptor.alphaMode = SurfaceAlphaMode::Straight;
    outCleanPlate.setColorDescriptor(cleanPlateDescriptor);
    return true;
}

} // namespace ArtifactCore::Keying
