module;
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <vector>
#include <algorithm>

module ImageProcessing;

import :VectorFlowGlitch;
import :StructureTensor;

namespace ArtifactCore {

void VectorFlowGlitch::process(ImageF32x4_RGBA& image, const VectorFlowGlitchSettings& settings) {
    if (image.isEmpty()) return;

    cv::Mat srcMat = image.toCVMat(); // CV_32FC4, BGRA format
    const int w = srcMat.cols;
    const int h = srcMat.rows;

    // 1. Analyze local structure tensors to obtain orientation fields
    StructureTensor tensor;
    // We use a relatively wide integration window to get clean consensus flow directions
    TensorField field = tensor.analyzeMat(&srcMat, 1.0f, 4.0f);

    cv::Mat dstMat = cv::Mat::zeros(h, w, CV_32FC4);

    // 2. Process each pixel and displace along structural direction
    for (int y = 0; y < h; ++y) {
        cv::Vec4f* dstRow = dstMat.ptr<cv::Vec4f>(y);

        for (int x = 0; x < w; ++x) {
            size_t idx = static_cast<size_t>(y * w + x);
            float angle = field.angles[idx];
            float coherence = field.coherence[idx];

            // Slicing noise function based on vertical coordinate and random seed
            float noiseFactor = std::sin(y * settings.frequency + settings.seed) * 
                                std::cos(y * (settings.frequency * 0.43f) - settings.seed * 3.14f);
            
            // Introduce sharp discontinuities (slicing spikes)
            if (std::abs(noiseFactor) < 0.2f) {
                // Skip displacement for non-glitched bands
                dstRow[x] = srcMat.at<cv::Vec4f>(y, x);
                continue;
            }

            // Direction vectors
            // Edges (flow vectors):
            float edgeVx = std::cos(angle);
            float edgeVy = std::sin(angle);
            // Default horizontal displacement:
            float defVx = 1.0f;
            float defVy = 0.0f;

            // Blend based on edge flow influence and local structural coherence
            float influence = settings.edgeFlowInfluence * coherence;
            float vx = (1.0f - influence) * defVx + influence * edgeVx;
            float vy = (1.0f - influence) * defVy + influence * edgeVy;

            // Normalize vector
            float len = std::sqrt(vx * vx + vy * vy);
            if (len > 0.0f) {
                vx /= len;
                vy /= len;
            }

            // Displacement shift
            float shift = noiseFactor * settings.glitchAmount;

            auto sampleChannel = [&](int chIdx, float customShift) -> float {
                float sx = x + (shift + customShift) * vx;
                float sy = y + (shift + customShift) * vy;

                // Clamp to boundary
                float cx = std::clamp(sx, 0.0f, static_cast<float>(w - 1));
                float cy = std::clamp(sy, 0.0f, static_cast<float>(h - 1));

                // Bilinear sampling
                int x0 = static_cast<int>(std::floor(cx));
                int x1 = std::min(x0 + 1, w - 1);
                int y0 = static_cast<int>(std::floor(cy));
                int y1 = std::min(y0 + 1, h - 1);

                float dx = cx - x0;
                float dy = cy - y0;

                const cv::Vec4f& p00 = srcMat.at<cv::Vec4f>(y0, x0);
                const cv::Vec4f& p10 = srcMat.at<cv::Vec4f>(y0, x1);
                const cv::Vec4f& p01 = srcMat.at<cv::Vec4f>(y1, x0);
                const cv::Vec4f& p11 = srcMat.at<cv::Vec4f>(y1, x1);

                return p00[chIdx] * ((1.0f - dx) * (1.0f - dy)) +
                       p10[chIdx] * (dx * (1.0f - dy)) +
                       p01[chIdx] * ((1.0f - dx) * dy) +
                       p11[chIdx] * (dx * dy);
            };

            // Displace channels individually to create chromatic aberration at tear edges
            // Note: BGRA layout -> 0 = B, 1 = G, 2 = R, 3 = A
            float b = sampleChannel(0, -settings.chromaticAberration);
            float g = sampleChannel(1, 0.0f);
            float r = sampleChannel(2, settings.chromaticAberration);
            float a = sampleChannel(3, 0.0f); // preserve original alpha coordinate displacement

            dstRow[x] = cv::Vec4f(b, g, r, a);
        }
    }

    image.setFromCVMat(dstMat);
}

} // namespace ArtifactCore
