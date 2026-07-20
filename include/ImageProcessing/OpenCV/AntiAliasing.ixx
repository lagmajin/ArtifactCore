module;
#include <utility>
#include <algorithm>
#include <cmath>
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
export module ImageProcessing:AntiAliasing;

export namespace ArtifactCore {

    /**
     * @brief FXAA-style anti-aliasing for rendered images.
     * Detects edges based on luminance contrast and smooths them.
     */
    LIBRARY_DLL_API inline cv::Mat antiAlias(const cv::Mat& input, float strength = 1.0f) {
        if (input.empty()) return input;
        if (!std::isfinite(strength)) strength = 1.0f;
        strength = std::clamp(strength, 0.0f, 4.0f);

        cv::Mat gray;
        if (input.channels() == 4) {
            cv::cvtColor(input, gray, cv::COLOR_BGRA2GRAY);
        } else if (input.channels() == 3) {
            cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = input.clone();
        }

        cv::Mat gray8;
        if (gray.depth() == CV_32F) {
            gray.convertTo(gray8, CV_8U, 255.0);
        } else {
            gray8 = gray;
        }

        cv::Mat gradX, gradY, absGradX, absGradY, edgeMask;
        cv::Sobel(gray8, gradX, CV_16S, 1, 0);
        cv::Sobel(gray8, gradY, CV_16S, 0, 1);
        cv::convertScaleAbs(gradX, absGradX);
        cv::convertScaleAbs(gradY, absGradY);
        cv::addWeighted(absGradX, 0.5, absGradY, 0.5, 0, edgeMask);

        cv::Mat edgeMaskFloat;
        edgeMask.convertTo(edgeMaskFloat, CV_32F, 1.0 / 255.0);
        cv::threshold(edgeMaskFloat, edgeMaskFloat, 0.05f, 1.0f,
                      cv::THRESH_BINARY);

        cv::Mat blurred;
        int kernelSize = static_cast<int>(strength * 2.0f) * 2 + 1;
        kernelSize = std::max(3, kernelSize);
        cv::GaussianBlur(input, blurred, cv::Size(kernelSize, kernelSize),
                         strength * 0.5);

        cv::Mat result = input.clone();
        const int channels = input.channels();
        for (int y = 0; y < input.rows; ++y) {
            for (int x = 0; x < input.cols; ++x) {
                const float mask =
                    std::min(1.0f, edgeMaskFloat.at<float>(y, x) * strength);
                if (input.depth() == CV_32F) {
                    for (int c = 0; c < channels; ++c) {
                        const float original = input.ptr<float>(y)[x * channels + c];
                        const float smooth = blurred.ptr<float>(y)[x * channels + c];
                        result.ptr<float>(y)[x * channels + c] =
                            original * (1.0f - mask) + smooth * mask;
                    }
                } else {
                    for (int c = 0; c < channels; ++c) {
                        const uchar original = input.ptr<uchar>(y)[x * channels + c];
                        const uchar smooth = blurred.ptr<uchar>(y)[x * channels + c];
                        result.ptr<uchar>(y)[x * channels + c] =
                            static_cast<uchar>(original * (1.0f - mask) +
                                               smooth * mask);
                    }
                }
            }
        }
        return result;
    }

    /**
     * @brief Supersampling anti-aliasing: render at higher res, then downsample.
     * factor: 2 = 2x2 SSAA, 4 = 4x4 SSAA
     */
    LIBRARY_DLL_API inline cv::Mat antiAliasSuperSample(
        const cv::Mat& input, int factor = 2) {
        if (input.empty() || factor < 2) return input;
        factor = std::clamp(factor, 2, 4);
        cv::Mat upscaled;
        cv::resize(input, upscaled,
                   cv::Size(input.cols * factor, input.rows * factor),
                   0, 0, cv::INTER_CUBIC);
        cv::Mat result;
        cv::resize(upscaled, result, input.size(), 0, 0, cv::INTER_AREA);
        return result;
    }

}
