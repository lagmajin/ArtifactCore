module;
#include <opencv2/opencv.hpp>
#include <cmath>
#include <algorithm>
module LiftCV;

namespace ArtifactCore {

cv::Mat liftGammaGain(const cv::Mat& input,
                       cv::Vec3f lift,
                       cv::Vec3f gamma,
                       cv::Vec3f gain) {
    if (input.empty()) return input;

    cv::Mat src;
    bool wasFloat = (input.depth() == CV_32F);
    if (!wasFloat) {
        input.convertTo(src, CV_32F, 1.0 / 255.0);
    } else {
        src = input.clone();
    }

    int ch = src.channels();
    cv::Mat result = src.clone();

    // Invert gamma for power function (gamma > 1 = darken midtones, < 1 = brighten)
    cv::Vec3f invGamma;
    for (int c = 0; c < 3; ++c) {
        invGamma[c] = (gamma[c] > 0.001f) ? (1.0f / gamma[c]) : 1000.0f;
    }

    for (int y = 0; y < src.rows; ++y) {
        for (int x = 0; x < src.cols; ++x) {
            for (int c = 0; c < std::min(ch, 3); ++c) {
                float val = src.ptr<float>(y)[x * ch + c];

                // Lift: offset shadows (adds to black level)
                val = val + lift[c] * (1.0f - val);

                // Gamma: midtone adjustment (power curve)
                val = std::pow(std::max(0.0f, val), invGamma[c]);

                // Gain: highlight multiplier
                val = val * gain[c];

                // Clamp
                result.ptr<float>(y)[x * ch + c] = std::min(1.0f, std::max(0.0f, val));
            }
            // Preserve alpha
            if (ch == 4) {
                result.ptr<float>(y)[x * ch + 3] = src.ptr<float>(y)[x * ch + 3];
            }
        }
    }

    if (!wasFloat) {
        result.convertTo(result, input.depth(), 255.0);
    }

    return result;
}

cv::Mat shadowLift(const cv::Mat& input, float amount) {
    return liftGammaGain(input,
                          cv::Vec3f(amount, amount, amount),
                          cv::Vec3f(1.0f, 1.0f, 1.0f),
                          cv::Vec3f(1.0f, 1.0f, 1.0f));
}

} // namespace ArtifactCore
