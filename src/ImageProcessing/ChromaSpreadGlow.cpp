module;
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

module ImageProcessing;

import :ChromaSpreadGlow;
import :ChromaSpread;
import Core.Parallel;
import Graphics.SurfaceColorContract;

namespace ArtifactCore {

void ChromaSpreadGlow::process(ImageF32x4_RGBA& image, const ChromaSpreadGlowSettings& settings) {
    if (image.isEmpty()) return;

    const auto descriptor = image.colorDescriptor();
    if (descriptor.channelOrder != SurfaceChannelOrder::RGBA &&
        descriptor.channelOrder != SurfaceChannelOrder::BGRA)
        return;
    cv::Mat srcMat = image.toCanonicalBGRA32FC4();
    const int w = srcMat.cols;
    const int h = srcMat.rows;
    const auto finiteOr = [](const float value, const float fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    const float threshold = std::clamp(finiteOr(settings.threshold, 0.5f),
                                       0.0f, 1.0f);
    const float glowRadius = std::clamp(finiteOr(settings.glowRadius, 0.0f),
                                        0.0f, 256.0f);
    const float intensity = std::max(0.0f, finiteOr(settings.intensity, 1.0f));
    const float aberrationScale = std::max(
        0.001f, finiteOr(settings.aberrationScale, 1.02f));
    const int dispersionSteps = std::clamp(settings.dispersionSteps, 1, 64);
    constexpr int redIndex = 2;
    constexpr int greenIndex = 1;
    constexpr int blueIndex = 0;
    const float tintB = finiteOr(settings.tintColor.x, 1.0f) * intensity;
    const float tintG = finiteOr(settings.tintColor.y, 1.0f) * intensity;
    const float tintR = finiteOr(settings.tintColor.z, 1.0f) * intensity;

    // 1. Extract Bright Areas (Thresholding)
    cv::Mat brightMat = cv::Mat::zeros(h, w, CV_32FC4);
    Parallel::ForPixels(0, h, w, h, [&](int y) {
        const cv::Vec4f* srcRow = srcMat.ptr<cv::Vec4f>(y);
        cv::Vec4f* brightRow = brightMat.ptr<cv::Vec4f>(y);
        for (int x = 0; x < w; ++x) {
            const cv::Vec4f& pixel = srcRow[x];
            const float luma = 0.299f * pixel[redIndex] +
                               0.587f * pixel[greenIndex] +
                               0.114f * pixel[blueIndex];

            if (luma >= threshold) {
                brightRow[x] = pixel;
            } else {
                brightRow[x] = cv::Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
    });

    // 2. Apply Gaussian Blur to the bright areas
    if (glowRadius > 0.0f) {
        int ksize = static_cast<int>(std::round(glowRadius * 3.0f)) * 2 + 1;
        cv::GaussianBlur(brightMat, brightMat, cv::Size(ksize, ksize), glowRadius);
    }

    // 3. Apply Chromatic Dispersion using ChromaSpread utility
    ChromaSpread dispersion;
    ChromaSpreadSettings dispersionSettings;
    dispersionSettings.redScale = aberrationScale;
    dispersionSettings.blueScale = std::max(0.001f, 2.0f - aberrationScale); // symmetrical scaling
    dispersionSettings.greenScale = 1.0f;
    dispersionSettings.dispersionSteps = dispersionSteps;
    dispersionSettings.shiftAmount = glowRadius * 0.1f; // proportional translation shift
    dispersionSettings.shiftAngle = 45.0f; // default shift direction angle

    dispersion.processMat(&brightMat, dispersionSettings);

    // 4. Tint & Additive Composite back onto srcMat
    Parallel::ForPixels(0, h, w, h, [&](int y) {
        cv::Vec4f* srcRow = srcMat.ptr<cv::Vec4f>(y);
        const cv::Vec4f* glowRow = brightMat.ptr<cv::Vec4f>(y);

        for (int x = 0; x < w; ++x) {
            cv::Vec4f& src = srcRow[x];
            const cv::Vec4f& glow = glowRow[x];

            // Additive blend for color channels, preserve original alpha
            src[redIndex] = std::clamp(
                src[redIndex] + glow[redIndex] * tintR, 0.0f, 1.0f);
            src[greenIndex] = std::clamp(
                src[greenIndex] + glow[greenIndex] * tintG, 0.0f, 1.0f);
            src[blueIndex] = std::clamp(
                src[blueIndex] + glow[blueIndex] * tintB, 0.0f, 1.0f);
        }
    });

    if (descriptor.channelOrder == SurfaceChannelOrder::RGBA) {
        cv::Mat rgbaMat;
        cv::cvtColor(srcMat, rgbaMat, cv::COLOR_BGRA2RGBA);
        image.setFromCVMat(rgbaMat, descriptor);
    } else {
        image.setFromCVMat(srcMat, descriptor);
    }
}

} // namespace ArtifactCore
