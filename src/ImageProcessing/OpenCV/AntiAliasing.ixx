module;
#include <utility>
#include <opencv2/opencv.hpp>
#include <random>
#include <cmath>
module ImageProcessing:AntiAliasing;

namespace ArtifactCore {

cv::Mat antiAlias(const cv::Mat& input, float strength) {
    if (input.empty()) return input;

    cv::Mat result;
    cv::Mat gray;

    // Convert to grayscale for edge detection
    if (input.channels() == 4) {
        cv::cvtColor(input, gray, cv::COLOR_BGRA2GRAY);
    } else if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = input.clone();
    }

    // Ensure 8-bit for edge detection
    cv::Mat gray8;
    if (gray.depth() == CV_32F) {
        gray.convertTo(gray8, CV_8U, 255.0);
    } else {
        gray8 = gray;
    }

    // Detect edges using Sobel
    cv::Mat gradX, gradY, edgeMask;
    cv::Sobel(gray8, gradX, CV_16S, 1, 0);
    cv::Sobel(gray8, gradY, CV_16S, 0, 1);
    cv::Mat absGradX, absGradY;
    cv::convertScaleAbs(gradX, absGradX);
    cv::convertScaleAbs(gradY, absGradY);
    cv::addWeighted(absGradX, 0.5, absGradY, 0.5, 0, edgeMask);

    // Normalize edge mask to 0..1
    cv::Mat edgeMaskFloat;
    edgeMask.convertTo(edgeMaskFloat, CV_32F, 1.0 / 255.0);

    // Apply threshold - only smooth where edges exist
    cv::threshold(edgeMaskFloat, edgeMaskFloat, 0.05f, 1.0f, cv::THRESH_BINARY);

    // Blur the input slightly
    cv::Mat blurred;
    int kernelSize = static_cast<int>(strength * 2.0f) * 2 + 1; // Odd kernel
    if (kernelSize < 3) kernelSize = 3;
    cv::GaussianBlur(input, blurred, cv::Size(kernelSize, kernelSize), strength * 0.5);

    // Blend: use blurred version only on edges, keep original elsewhere
    result = input.clone();
    int ch = input.channels();
    for (int y = 0; y < input.rows; ++y) {
        for (int x = 0; x < input.cols; ++x) {
            float mask = edgeMaskFloat.at<float>(y, x) * strength;
            mask = std::min(1.0f, mask);

            if (input.depth() == CV_32F) {
                for (int c = 0; c < ch; ++c) {
                    float orig = input.ptr<float>(y)[x * ch + c];
                    float blur = blurred.ptr<float>(y)[x * ch + c];
                    result.ptr<float>(y)[x * ch + c] = orig * (1.0f - mask) + blur * mask;
                }
            } else {
                for (int c = 0; c < ch; ++c) {
                    uchar orig = input.ptr<uchar>(y)[x * ch + c];
                    uchar blur = blurred.ptr<uchar>(y)[x * ch + c];
                    result.ptr<uchar>(y)[x * ch + c] = static_cast<uchar>(orig * (1.0f - mask) + blur * mask);
                }
            }
        }
    }

    return result;
}

cv::Mat antiAliasSuperSample(const cv::Mat& input, int factor) {
    if (input.empty() || factor < 2) return input;

    // Upscale
    cv::Mat upscaled;
    cv::resize(input, upscaled, cv::Size(input.cols * factor, input.rows * factor), 0, 0, cv::INTER_CUBIC);

    // Downscale with area interpolation (averaging)
    cv::Mat result;
    cv::resize(upscaled, result, input.size(), 0, 0, cv::INTER_AREA);

    return result;
}

} // namespace ArtifactCore
