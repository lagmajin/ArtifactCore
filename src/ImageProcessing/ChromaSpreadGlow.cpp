module;
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <vector>

module ImageProcessing;

import :ChromaSpreadGlow;
import :ChromaSpread;

namespace ArtifactCore {

void ChromaSpreadGlow::process(ImageF32x4_RGBA& image, const ChromaSpreadGlowSettings& settings) {
    if (image.isEmpty()) return;

    cv::Mat srcMat = image.toCVMat(); // CV_32FC4, BGRA
    const int w = srcMat.cols;
    const int h = srcMat.rows;

    // 1. Extract Bright Areas (Thresholding)
    cv::Mat brightMat = cv::Mat::zeros(h, w, CV_32FC4);
    for (int y = 0; y < h; ++y) {
        const cv::Vec4f* srcRow = srcMat.ptr<cv::Vec4f>(y);
        cv::Vec4f* brightRow = brightMat.ptr<cv::Vec4f>(y);
        for (int x = 0; x < w; ++x) {
            const cv::Vec4f& pixel = srcRow[x];
            // Simple luminance calculation: Y = 0.299*R + 0.587*G + 0.114*B
            // ImageF32x4_RGBA uses BGRA internally: pixel[0]=B, pixel[1]=G, pixel[2]=R
            float luma = 0.299f * pixel[2] + 0.587f * pixel[1] + 0.114f * pixel[0];

            if (luma >= settings.threshold) {
                brightRow[x] = pixel;
            } else {
                brightRow[x] = cv::Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
    }

    // 2. Apply Gaussian Blur to the bright areas
    if (settings.glowRadius > 0.0f) {
        int ksize = static_cast<int>(std::round(settings.glowRadius * 3.0f)) * 2 + 1;
        cv::GaussianBlur(brightMat, brightMat, cv::Size(ksize, ksize), settings.glowRadius);
    }

    // 3. Apply Chromatic Dispersion using ChromaSpread utility
    ChromaSpread dispersion;
    ChromaSpreadSettings dispersionSettings;
    dispersionSettings.redScale = settings.aberrationScale;
    dispersionSettings.blueScale = 2.0f - settings.aberrationScale; // symmetrical scaling
    dispersionSettings.greenScale = 1.0f;
    dispersionSettings.dispersionSteps = settings.dispersionSteps;
    dispersionSettings.shiftAmount = settings.glowRadius * 0.1f; // proportional translation shift
    dispersionSettings.shiftAngle = 45.0f; // default shift direction angle

    dispersion.processMat(&brightMat, dispersionSettings);

    // 4. Tint & Additive Composite back onto srcMat
    float tintB = settings.tintColor.x * settings.intensity; // Tint RGBA
    float tintG = settings.tintColor.y * settings.intensity;
    float tintR = settings.tintColor.z * settings.intensity;

    for (int y = 0; y < h; ++y) {
        cv::Vec4f* srcRow = srcMat.ptr<cv::Vec4f>(y);
        const cv::Vec4f* glowRow = brightMat.ptr<cv::Vec4f>(y);

        for (int x = 0; x < w; ++x) {
            cv::Vec4f& src = srcRow[x];
            const cv::Vec4f& glow = glowRow[x];

            // Additive blend for color channels, preserve original alpha
            src[0] = std::clamp(src[0] + glow[0] * tintB, 0.0f, 1.0f); // Blue
            src[1] = std::clamp(src[1] + glow[1] * tintG, 0.0f, 1.0f); // Green
            src[2] = std::clamp(src[2] + glow[2] * tintR, 0.0f, 1.0f); // Red
        }
    }

    image.setFromCVMat(srcMat);
}

} // namespace ArtifactCore
