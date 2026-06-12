module;
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <vector>
#include <algorithm>

module ImageProcessing;

import :AnisotropicFlowBlur;
import :StructureTensor;

namespace ArtifactCore {

void AnisotropicFlowBlur::process(ImageF32x4_RGBA& image, const AnisotropicFlowBlurSettings& settings) {
    if (image.isEmpty()) return;

    cv::Mat srcMat = image.toCVMat(); // CV_32FC4, BGRA
    const int w = srcMat.cols;
    const int h = srcMat.rows;

    // 1. Analyze structure tensor to get local angles & coherence
    StructureTensor tensor;
    TensorField field = tensor.analyzeMat(&srcMat, settings.tensorNoiseScale, settings.tensorIntegrationScale);

    cv::Mat dstMat = cv::Mat::zeros(h, w, CV_32FC4);

    // 2. Perform directional anisotropic filtering per pixel
    const int steps = 9; // Number of samples along the line (must be odd)
    const int halfSteps = steps / 2;

    for (int y = 0; y < h; ++y) {
        cv::Vec4f* dstRow = dstMat.ptr<cv::Vec4f>(y);
        for (int x = 0; x < w; ++x) {
            size_t idx = static_cast<size_t>(y * w + x);
            float angle = field.angles[idx];
            float coherence = field.coherence[idx];

            // Blur radius scaling based on coherence
            // Along the flow vector: max blur amount
            // Across the flow vector: reduced blur amount
            float u_len = settings.blurAmount;
            // Coherence scales from 0 (isotropic) to 1 (highly anisotropic edge)
            float v_len = settings.blurAmount * (1.0f - settings.edgeAdherence * coherence);

            if (u_len <= 0.5f) {
                // Return original pixel if no blur is applied
                dstRow[x] = srcMat.at<cv::Vec4f>(y, x);
                continue;
            }

            // Direction vectors
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);

            // Accumulate weighted samples along the flow direction (Line Integral Convolution style)
            cv::Vec4f accum(0.0f, 0.0f, 0.0f, 0.0f);
            float weightSum = 0.0f;

            for (int i = -halfSteps; i <= halfSteps; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(halfSteps); // -1.0 to 1.0
                float offsetDist = t * u_len;

                // Target coordinate
                float sx = x + offsetDist * cosA;
                float sy = y + offsetDist * sinA;

                // Clamp to image boundaries
                float clampedX = std::clamp(sx, 0.0f, static_cast<float>(w - 1));
                float clampedY = std::clamp(sy, 0.0f, static_cast<float>(h - 1));

                // Bilinear Interpolation
                int x0 = static_cast<int>(std::floor(clampedX));
                int x1 = std::min(x0 + 1, w - 1);
                int y0 = static_cast<int>(std::floor(clampedY));
                int y1 = std::min(y0 + 1, h - 1);

                float dx = clampedX - x0;
                float dy = clampedY - y0;

                cv::Vec4f p00 = srcMat.at<cv::Vec4f>(y0, x0);
                cv::Vec4f p10 = srcMat.at<cv::Vec4f>(y0, x1);
                cv::Vec4f p01 = srcMat.at<cv::Vec4f>(y1, x0);
                cv::Vec4f p11 = srcMat.at<cv::Vec4f>(y1, x1);

                cv::Vec4f interpolated = p00 * ((1.0f - dx) * (1.0f - dy)) +
                                         p10 * (dx * (1.0f - dy)) +
                                         p01 * ((1.0f - dx) * dy) +
                                         p11 * (dx * dy);

                // Gaussian weight
                float weight = std::exp(-(t * t) * 1.5f);
                accum += interpolated * weight;
                weightSum += weight;
            }

            if (weightSum > 0.0f) {
                dstRow[x] = accum / weightSum;
            } else {
                dstRow[x] = srcMat.at<cv::Vec4f>(y, x);
            }
        }
    }

    image.setFromCVMat(dstMat);
}

} // namespace ArtifactCore
