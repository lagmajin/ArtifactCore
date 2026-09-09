module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
#include <opencv2/opencv.hpp>

module ImageProcessing;
import :FilmRepair;
import Core.Parallel;

namespace ArtifactCore::Repair {
namespace {

float finiteOr(float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
}

float smooth01(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

cv::Mat channelLuma(const cv::Mat& rgba) {
    std::vector<cv::Mat> channels;
    cv::split(rgba, channels);
    return channels[0] * 0.2126f + channels[1] * 0.7152f +
        channels[2] * 0.0722f;
}

} // namespace

bool processFilmRepair(const FilmRepairBuffers& buffers,
                       const FilmRepairParams& inputParams) {
    if (!buffers.src || !buffers.dst || buffers.src == buffers.dst ||
        buffers.width <= 0 || buffers.height <= 0)
        return false;
    const auto width = static_cast<std::size_t>(buffers.width);
    const auto height = static_cast<std::size_t>(buffers.height);
    if (height != 0 && width > std::numeric_limits<std::size_t>::max() / height)
        return false;
    const std::size_t pixelCount = width * height;
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4)
        return false;
    if (buffers.prev == buffers.dst || buffers.next == buffers.dst ||
        buffers.namedMask == buffers.dst)
        return false;
    const int w = buffers.width;
    const int h = buffers.height;

    FilmRepairParams p = inputParams;
    p.threshold = std::clamp(finiteOr(p.threshold, 0.12f), 0.0f, 2.0f);
    p.softness = std::max(1.0e-5f, finiteOr(p.softness, 0.05f));
    p.amount = std::clamp(finiteOr(p.amount, 1.0f), 0.0f, 2.0f);
    p.mix = std::clamp(finiteOr(p.mix, 1.0f), 0.0f, 1.0f);
    p.radius = std::clamp(p.radius, 1, 2);
    p.temporalThreshold = std::clamp(finiteOr(p.temporalThreshold, 0.08f), 0.0f, 1.0f);
    p.motionReject = std::clamp(finiteOr(p.motionReject, 0.5f), 0.0f, 2.0f);
    p.scratchThreshold = std::clamp(finiteOr(p.scratchThreshold, 0.1f), 0.0f, 1.0f);
    p.scratchMinLength = std::clamp(p.scratchMinLength, 4, 256);

    const float* source = buffers.src;
    cv::Mat rgba(h, w, CV_32FC4, const_cast<float*>(source));
    std::vector<cv::Mat> channels;
    cv::split(rgba, channels);
    std::vector<cv::Mat> medianChannels(3);
    const int kernel = p.radius * 2 + 1;
    for (int channel = 0; channel < 3; ++channel)
        cv::medianBlur(channels[channel], medianChannels[channel], kernel);
    const cv::Mat luma = channelLuma(rgba);
    const cv::Mat medianLuma = medianChannels[0] * 0.2126f +
        medianChannels[1] * 0.7152f + medianChannels[2] * 0.0722f;

    std::vector<float> mask(pixelCount, 0.0f);
    // Per-pixel temporal weight: drives neighbor-average fill when
    // temporalRepair is enabled, otherwise stays zero (median fill).
    std::vector<float> temporalWeight(pixelCount, 0.0f);
    if (!p.namedMaskOnly) {
        Parallel::ForTiles(w, h, 32, 32, [&](int x0, int y0, int x1, int y1) {
            for (int y = y0; y < y1; ++y) {
                const float* lumaRow = luma.ptr<float>(y);
                const float* medianRow = medianLuma.ptr<float>(y);
                for (int x = x0; x < x1; ++x) {
                    const float residual = lumaRow[x] - medianRow[x];
                    const bool eligible =
                        (residual >= 0.0f && p.repairBright) ||
                        (residual < 0.0f && p.repairDark);
                    if (!eligible) continue;
                    const float normalized =
                        (std::abs(residual) - p.threshold) / p.softness;
                    mask[static_cast<std::size_t>(y) * width +
                         static_cast<std::size_t>(x)] = smooth01(normalized);
                }
            }
        });

        // Temporal single-frame dirt: differs from BOTH neighbors while the
        // neighbors agree. Neighbor disagreement marks motion and rejects.
        if (p.temporalDetect && buffers.prev && buffers.next) {
            const cv::Mat prevLuma = channelLuma(
                cv::Mat(h, w, CV_32FC4, const_cast<float*>(buffers.prev)));
            const cv::Mat nextLuma = channelLuma(
                cv::Mat(h, w, CV_32FC4, const_cast<float*>(buffers.next)));
            const float agree = std::max(p.temporalThreshold * 0.5f, 1.0e-4f);
            Parallel::ForTiles(w, h, 32, 32, [&](int x0, int y0, int x1, int y1) {
                for (int y = y0; y < y1; ++y) {
                    const float* lumaRow = luma.ptr<float>(y);
                    const float* prevRow = prevLuma.ptr<float>(y);
                    const float* nextRow = nextLuma.ptr<float>(y);
                    for (int x = x0; x < x1; ++x) {
                        const float toPrev = lumaRow[x] - prevRow[x];
                        const float toNext = lumaRow[x] - nextRow[x];
                        if (toPrev * toNext <= 0.0f) continue;
                        const float magnitude =
                            std::min(std::abs(toPrev), std::abs(toNext));
                        if (magnitude < p.temporalThreshold) continue;
                        const bool eligible =
                            (toPrev >= 0.0f && p.repairBright) ||
                            (toPrev < 0.0f && p.repairDark);
                        if (!eligible) continue;
                        const float neighborDiff = std::abs(prevRow[x] - nextRow[x]);
                        if (neighborDiff > p.motionReject) continue;
                        const float gate =
                            1.0f - std::clamp(neighborDiff / agree, 0.0f, 1.0f);
                        const float normalized =
                            (magnitude - p.temporalThreshold) / p.softness;
                        const float weight =
                            smooth01(normalized) * std::clamp(gate, 0.0f, 1.0f);
                        const std::size_t slotIdx = static_cast<std::size_t>(y) * width +
                            static_cast<std::size_t>(x);
                        float& slot = mask[slotIdx];
                        slot = std::max(slot, weight);
                        if (p.temporalRepair) {
                            float& tw = temporalWeight[slotIdx];
                            tw = std::max(tw, weight);
                        }
                    }
                }
            });
        }

        // Vertical scratches persist across frames at fixed x: isolate them
        // with a vertical white/black top-hat instead of temporal logic.
        if (p.scratchDetect) {
            const cv::Mat verticalKernel = cv::getStructuringElement(
                cv::MORPH_RECT, cv::Size(1, p.scratchMinLength));
            cv::Mat opened, closed;
            cv::morphologyEx(luma, opened, cv::MORPH_OPEN, verticalKernel);
            cv::morphologyEx(luma, closed, cv::MORPH_CLOSE, verticalKernel);
            const cv::Mat whiteHat = luma - opened;
            const cv::Mat blackHat = closed - luma;
            const int dilateWidth = std::clamp(p.radius * 2 + 1, 1, 9);
            for (int y = 0; y < h; ++y) {
                const float* whiteRow = whiteHat.ptr<float>(y);
                const float* blackRow = blackHat.ptr<float>(y);
                for (int x = 0; x < w; ++x) {
                    float response = 0.0f;
                    if (p.repairBright)
                        response = std::max(response, whiteRow[x]);
                    if (p.repairDark)
                        response = std::max(response, blackRow[x]);
                    const float normalized =
                        (response - p.scratchThreshold) / p.softness;
                    float weight = smooth01(normalized);
                    // Widen to the repair radius so the median fill covers
                    // line edges (1D max over the widen window).
                    if (dilateWidth > 1 && weight > 0.0f) {
                        const int x0 = std::max(0, x - dilateWidth / 2);
                        const int x1 = std::min(w - 1, x + dilateWidth / 2);
                        for (int sx = x0; sx <= x1; ++sx) {
                            float neighbor = 0.0f;
                            if (p.repairBright)
                                neighbor = std::max(neighbor, whiteHat.at<float>(y, sx));
                            if (p.repairDark)
                                neighbor = std::max(neighbor, blackHat.at<float>(y, sx));
                            weight = std::max(
                                weight, smooth01((neighbor - p.scratchThreshold) / p.softness));
                        }
                    }
                    float& slot = mask[static_cast<std::size_t>(y) * width +
                                       static_cast<std::size_t>(x)];
                    slot = std::max(slot, weight);
                }
            }
        }
    }

    if (buffers.namedMask) {
        if (p.namedMaskOnly) {
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const float m = buffers.namedMask[i];
                mask[i] = std::isfinite(m) ? std::clamp(m, 0.0f, 1.0f) : 0.0f;
            }
        } else {
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const float m = buffers.namedMask[i];
                if (std::isfinite(m)) mask[i] = std::max(mask[i], std::clamp(m, 0.0f, 1.0f));
            }
        }
    }
    bool hasMask = false;
    for (const float m : mask) {
        if (m > 0.0001f) {
            hasMask = true;
            break;
        }
    }
    if (!hasMask) return false;
    float* output = buffers.dst;
    const bool useNeighborFill =
        p.temporalRepair && buffers.prev && buffers.next;
    Parallel::ForTiles(w, h, 32, 32, [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* medianRows[3] = {medianChannels[0].ptr<float>(y),
                                          medianChannels[1].ptr<float>(y),
                                          medianChannels[2].ptr<float>(y)};
            for (int x = x0; x < x1; ++x) {
                const std::size_t idx = static_cast<std::size_t>(y) * width +
                    static_cast<std::size_t>(x);
                const std::size_t offset = idx * 4;
                const float blend =
                    std::clamp(mask[idx] * p.amount * p.mix, 0.0f, 1.0f);
                const float neighborBlend = useNeighborFill
                    ? std::clamp(temporalWeight[idx], 0.0f, 1.0f)
                    : 0.0f;
                for (int channel = 0; channel < 3; ++channel) {
                    float fill = medianRows[channel][x];
                    if (neighborBlend > 0.0f) {
                        const float navg =
                            (buffers.prev[offset + channel] +
                             buffers.next[offset + channel]) * 0.5f;
                        fill = std::lerp(fill, navg, neighborBlend);
                    }
                    output[offset + channel] = std::lerp(source[offset + channel],
                                                         fill, blend);
                }
                output[offset + 3] = source[offset + 3];
            }
        }
    });
    return true;
}

double frameLuma(const float* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) return std::numeric_limits<double>::quiet_NaN();
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (h != 0 && w > std::numeric_limits<std::size_t>::max() / h)
        return std::numeric_limits<double>::quiet_NaN();
    // Subsampled mean (every 4th row/column), non-finite samples skipped.
    double total = 0.0;
    std::size_t count = 0;
    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width; x += 4) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x)) * 4;
            const double r = rgba[offset + 0];
            const double g = rgba[offset + 1];
            const double b = rgba[offset + 2];
            if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b))
                continue;
            total += r * 0.299 + g * 0.587 + b * 0.114;
            ++count;
        }
    }
    if (count == 0) return std::numeric_limits<double>::quiet_NaN();
    return total / static_cast<double>(count);
}

double deblinkGain(double curLuma, double prevLuma, double nextLuma,
                   const DeBlinkParams& inputParams, bool* applied) {
    if (applied) *applied = false;
    if (!std::isfinite(curLuma) || !std::isfinite(prevLuma) ||
        !std::isfinite(nextLuma))
        return 1.0;
    DeBlinkParams p = inputParams;
    if (!std::isfinite(p.threshold) || !std::isfinite(p.strength) ||
        !std::isfinite(p.sceneCut) || !std::isfinite(p.maxGain))
        return 1.0;
    p.threshold = std::clamp(p.threshold, 0.0, 1.0);
    p.strength = std::clamp(p.strength, 0.0, 1.0);
    p.sceneCut = std::clamp(p.sceneCut, 0.0, 2.0);
    p.maxGain = std::clamp(p.maxGain, 1.0, 16.0);
    // Must deviate from BOTH neighbors: single-frame anomaly only.
    const double devPrev = std::abs(curLuma - prevLuma);
    const double devNext = std::abs(curLuma - nextLuma);
    if (std::min(devPrev, devNext) < p.threshold) return 1.0;
    // Neighbors must agree; otherwise this is a cut or dissolve, not a blink.
    if (std::abs(prevLuma - nextLuma) > p.sceneCut) return 1.0;
    const double target = (prevLuma + nextLuma) * 0.5;
    if (target <= 1.0e-6) return 1.0;
    double gain = std::clamp(target / std::max(curLuma, 1.0e-4),
                             1.0 / p.maxGain, p.maxGain);
    gain = 1.0 + (gain - 1.0) * p.strength;
    if (applied) *applied = true;
    return gain;
}

} // namespace ArtifactCore::Repair
