module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
#include <opencv2/opencv.hpp>

module ImageProcessing;
import :WireRemove;
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

float sampleBilinear(const float* pixels, int width, int height,
                     float x, float y, int channel) {
    x = std::clamp(x, 0.0f, static_cast<float>(width - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(height - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - x0;
    const float ty = y - y0;
    const auto valueAt = [&](int px, int py) {
        return pixels[(static_cast<std::size_t>(py) * width + px) * 4u + channel];
    };
    const float top = std::lerp(valueAt(x0, y0), valueAt(x1, y0), tx);
    const float bottom = std::lerp(valueAt(x0, y1), valueAt(x1, y1), tx);
    return std::lerp(top, bottom, ty);
}

struct SegmentPixels {
    cv::Point2f start;
    cv::Point2f end;
    cv::Point2f normal = {1.0f, 0.0f};
    float lengthSquared = 0.0f;
    float length = 0.0f;
    bool valid = false;
};

} // namespace

bool processWireRemove(const WireRemoveBuffers& buffers,
                       const WireRemoveParams& inputParams) {
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
    if (buffers.clean == buffers.dst || buffers.namedMask == buffers.dst)
        return false;
    const int w = buffers.width;
    const int h = buffers.height;

    WireRemoveParams p = inputParams;
    p.width = std::clamp(finiteOr(p.width, 8.0f), 0.5f, 256.0f);
    p.feather = std::clamp(finiteOr(p.feather, 5.0f), 0.0f, 128.0f);
    p.cloneOffsetX = std::clamp(finiteOr(p.cloneOffsetX, 32.0f), -2048.0f, 2048.0f);
    p.cloneOffsetY = std::clamp(finiteOr(p.cloneOffsetY, 0.0f), -2048.0f, 2048.0f);
    p.mix = std::clamp(finiteOr(p.mix, 1.0f), 0.0f, 1.0f);
    p.inpaintRadius = std::clamp(finiteOr(p.inpaintRadius, 4.0f), 1.0f, 24.0f);
    p.method = std::clamp(p.method, 0, 4);
    for (auto& seg : p.segs) {
        seg.x1 = finiteOr(seg.x1, 0.0f);
        seg.y1 = finiteOr(seg.y1, 0.0f);
        seg.x2 = finiteOr(seg.x2, 0.0f);
        seg.y2 = finiteOr(seg.y2, 0.0f);
    }

    SegmentPixels segs[3];
    int activeCount = 0;
    for (int i = 0; i < 3; ++i) {
        if (!p.segs[i].enabled) continue;
        auto& s = segs[i];
        s.start = cv::Point2f(p.segs[i].x1 * (w - 1), p.segs[i].y1 * (h - 1));
        s.end = cv::Point2f(p.segs[i].x2 * (w - 1), p.segs[i].y2 * (h - 1));
        const cv::Point2f direction = s.end - s.start;
        s.lengthSquared = direction.dot(direction);
        s.length = std::sqrt(std::max(s.lengthSquared, 1.0e-8f));
        s.normal = cv::Point2f(-direction.y / s.length, direction.x / s.length);
        s.valid = s.lengthSquared > 1.0e-8f;
        if (s.valid) ++activeCount;
    }

    const float innerRadius = p.width * 0.5f;
    const float outerRadius = innerRadius + p.feather;
    std::vector<float> mask(pixelCount, 0.0f);
    // Nearest-segment normal per pixel for the side-average fill. Defaults
    // to horizontal so mask-only pixels still fill sanely.
    std::vector<float> normalX(pixelCount, 1.0f);
    std::vector<float> normalY(pixelCount, 0.0f);
    if (activeCount > 0) {
        Parallel::ForTiles(w, h, 16, 16, [&](int x0, int y0, int x1, int y1) {
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    const cv::Point2f point(static_cast<float>(x),
                                            static_cast<float>(y));
                    float best = 0.0f;
                    cv::Point2f bestNormal(1.0f, 0.0f);
                    for (int i = 0; i < 3; ++i) {
                        const auto& s = segs[i];
                        if (!s.valid) continue;
                        const float t = std::clamp(
                            (point - s.start).dot(s.end - s.start) / s.lengthSquared,
                            0.0f, 1.0f);
                        const float distance =
                            cv::norm(point - (s.start + (s.end - s.start) * t));
                        float weight = 0.0f;
                        if (distance <= innerRadius) weight = 1.0f;
                        else if (distance < outerRadius && p.feather > 0.0f)
                            weight = 1.0f - smooth01((distance - innerRadius) / p.feather);
                        if (weight > best) {
                            best = weight;
                            bestNormal = s.normal;
                        }
                    }
                    const std::size_t idx = static_cast<std::size_t>(y) * width +
                        static_cast<std::size_t>(x);
                    mask[idx] = best;
                    normalX[idx] = bestNormal.x;
                    normalY[idx] = bestNormal.y;
                }
            }
        });
    }
    if (buffers.namedMask) {
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const float m = buffers.namedMask[i];
            if (std::isfinite(m)) mask[i] = std::max(mask[i], std::clamp(m, 0.0f, 1.0f));
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

    const float* source = buffers.src;
    float* output = buffers.dst;
    const bool hasReplacement = buffers.clean != nullptr;

    cv::Mat blurredSource;
    if (p.method == 2 && !hasReplacement) {
        cv::GaussianBlur(cv::Mat(h, w, CV_32FC4, const_cast<float*>(source)),
                         blurredSource, cv::Size(), std::max(1.0f, p.width));
    }

    cv::Mat inpainted;
    bool hasInpaint = false;
    if ((p.method == 3 || p.method == 4) && !hasReplacement) {
        std::vector<cv::Mat> channels;
        cv::split(cv::Mat(h, w, CV_32FC4, const_cast<float*>(source)), channels);
        const cv::Mat sourceAlpha = channels[3].clone();
        cv::Mat rgb8, mask8;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgb8);
        rgb8.convertTo(rgb8, CV_8UC3, 255.0);
        cv::Mat maskMat(h, w, CV_32F, mask.data());
        cv::compare(maskMat, 0.001, mask8, cv::CMP_GT);
        if (cv::countNonZero(mask8) > 0) {
            cv::Mat filled8;
            cv::inpaint(rgb8, mask8, filled8, static_cast<double>(p.inpaintRadius),
                        p.method == 3 ? cv::INPAINT_TELEA : cv::INPAINT_NS);
            filled8.convertTo(filled8, CV_32FC3, 1.0 / 255.0);
            cv::split(filled8, channels);
            channels.push_back(sourceAlpha);
            cv::merge(channels, inpainted);
            hasInpaint = !inpainted.empty();
        }
    }

    const float sideDistance = p.width * 0.5f + p.feather + 2.0f;
    Parallel::ForTiles(w, h, 32, 32, [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const std::size_t idx = static_cast<std::size_t>(y) * width +
                    static_cast<std::size_t>(x);
                const std::size_t offset = idx * 4;
                const float amount = std::clamp(mask[idx] * p.mix, 0.0f, 1.0f);
                if (amount <= 0.0f) {
                    if (p.viewMask) {
                        output[offset + 0] = 0.0f;
                        output[offset + 1] = 0.0f;
                        output[offset + 2] = 0.0f;
                        output[offset + 3] = 1.0f;
                    } else {
                        output[offset + 0] = source[offset + 0];
                        output[offset + 1] = source[offset + 1];
                        output[offset + 2] = source[offset + 2];
                        output[offset + 3] = source[offset + 3];
                    }
                    continue;
                }
                for (int channel = 0; channel < 3; ++channel) {
                    float fill = source[offset + channel];
                    if (hasReplacement) {
                        fill = buffers.clean[offset + channel];
                    } else if (hasInpaint) {
                        fill = inpainted.at<cv::Vec4f>(y, x)[channel];
                    } else if (p.method == 0) {
                        const float nx = normalX[idx];
                        const float ny = normalY[idx];
                        const float a = sampleBilinear(
                            source, w, h, x + nx * sideDistance,
                            y + ny * sideDistance, channel);
                        const float b = sampleBilinear(
                            source, w, h, x - nx * sideDistance,
                            y - ny * sideDistance, channel);
                        fill = (a + b) * 0.5f;
                    } else if (p.method == 2) {
                        fill = blurredSource.at<cv::Vec4f>(y, x)[channel];
                    } else {
                        fill = sampleBilinear(source, w, h, x + p.cloneOffsetX,
                                              y + p.cloneOffsetY, channel);
                    }
                    output[offset + channel] =
                        std::lerp(source[offset + channel], fill, amount);
                }
                output[offset + 3] = source[offset + 3];
                if (p.viewMask) {
                    const float m = mask[idx];
                    output[offset + 0] = m;
                    output[offset + 1] = m;
                    output[offset + 2] = m;
                    output[offset + 3] = 1.0f;
                }
            }
        }
    });
    return true;
}

} // namespace ArtifactCore::Repair
