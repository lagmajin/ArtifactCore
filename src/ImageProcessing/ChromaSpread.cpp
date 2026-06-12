module;
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <vector>

module ImageProcessing;

import :ChromaSpread;

namespace ArtifactCore {

void ChromaSpread::process(ImageF32x4_RGBA& image, const ChromaSpreadSettings& settings) {
    if (image.isEmpty()) return;
    cv::Mat mat = image.toCVMat(); // returns CV_32FC4 in BGRA format
    processMat(&mat, settings);
    image.setFromCVMat(mat);
}

void ChromaSpread::processMat(void* cvMatPtr, const ChromaSpreadSettings& settings) {
    if (!cvMatPtr) return;
    cv::Mat& srcMat = *static_cast<cv::Mat*>(cvMatPtr);
    if (srcMat.empty() || srcMat.type() != CV_32FC4) return;

    const int w = srcMat.cols;
    const int h = srcMat.rows;
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;

    // Split into B, G, R, A channels
    // Note: ImageF32x4_RGBA uses BGRA layout internally!
    std::vector<cv::Mat> channels(4);
    cv::split(srcMat, channels);

    cv::Mat r_src = channels[2];
    cv::Mat g_src = channels[1];
    cv::Mat b_src = channels[0];
    cv::Mat a_src = channels[3];

    // Destination maps for output
    cv::Mat r_dst = cv::Mat::zeros(h, w, CV_32FC1);
    cv::Mat g_dst = cv::Mat::zeros(h, w, CV_32FC1);
    cv::Mat b_dst = cv::Mat::zeros(h, w, CV_32FC1);

    // Calculate linear shift vectors
    const float rad = settings.shiftAngle * 3.14159265f / 180.0f;
    const float shiftX = settings.shiftAmount * std::cos(rad);
    const float shiftY = settings.shiftAmount * std::sin(rad);

    auto remapChannel = [&](const cv::Mat& src, cv::Mat& dst, float scale, float sx, float sy) {
        cv::Mat mapX(h, w, CV_32FC1);
        cv::Mat mapY(h, w, CV_32FC1);

        for (int y = 0; y < h; ++y) {
            float* ptrX = mapX.ptr<float>(y);
            float* ptrY = mapY.ptr<float>(y);
            for (int x = 0; x < w; ++x) {
                // Radial scaling centered around (cx, cy)
                float rx = (x - cx) * scale + cx;
                float ry = (y - cy) * scale + cy;
                
                // Add linear shift offset
                ptrX[x] = rx + sx;
                ptrY[x] = ry + sy;
            }
        }
        cv::remap(src, dst, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    };

    if (settings.dispersionSteps <= 1) {
        // Direct transform
        // Red: shift positive, scale Red
        remapChannel(r_src, r_dst, settings.redScale, shiftX, shiftY);
        // Green: centered, scale Green (usually 1.0)
        remapChannel(g_src, g_dst, settings.greenScale, 0.0f, 0.0f);
        // Blue: shift negative, scale Blue
        remapChannel(b_src, b_dst, settings.blueScale, -shiftX, -shiftY);
    } else {
        // Multi-spectral dispersion interpolation
        // Blend intermediate steps along the dispersion axis to generate a smooth prism trail.
        cv::Mat r_accum = cv::Mat::zeros(h, w, CV_32FC1);
        cv::Mat g_accum = cv::Mat::zeros(h, w, CV_32FC1);
        cv::Mat b_accum = cv::Mat::zeros(h, w, CV_32FC1);

        const int steps = settings.dispersionSteps;
        for (int i = 0; i < steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps - 1); // 0.0 to 1.0

            // Interpolate scaling and shifting factors across spectrum
            float curScale = settings.redScale * (1.0f - t) + settings.blueScale * t;
            float curSx = shiftX * (1.0f - 2.0f * t);
            float curSy = shiftY * (1.0f - 2.0f * t);

            // Generate virtual wavelength colors for interpolation (approximate prism refraction spectrum)
            // t = 0 (Red) -> t = 0.5 (Green) -> t = 1.0 (Blue)
            float factorR = std::max(0.0f, 1.0f - 2.0f * std::abs(t - 0.0f));
            float factorG = std::max(0.0f, 1.0f - 2.0f * std::abs(t - 0.5f));
            float factorB = std::max(0.0f, 1.0f - 2.0f * std::abs(t - 1.0f));

            // Normalize weights
            float sum = factorR + factorG + factorB;
            if (sum > 0.0f) {
                factorR /= sum;
                factorG /= sum;
                factorB /= sum;
            }

            cv::Mat tmp;
            // Sample source (we use a blend of channels based on spectrum position for dispersion)
            cv::Mat srcMixed = r_src * factorR + g_src * factorG + b_src * factorB;
            remapChannel(srcMixed, tmp, curScale, curSx, curSy);

            // R, G, B channels reconstructed from the spectrum
            r_accum += tmp * factorR;
            g_accum += tmp * factorG;
            b_accum += tmp * factorB;
        }

        r_dst = r_accum;
        g_dst = g_accum;
        b_dst = b_accum;
    }

    // Replace original color channels (preserve Alpha)
    channels[2] = r_dst;
    channels[1] = g_dst;
    channels[0] = b_dst;

    cv::merge(channels, srcMat);
}

} // namespace ArtifactCore
