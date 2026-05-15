module;
#include <utility>
#include <random>
#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>
module ImageProcessing.GlitchCV;

namespace ArtifactCore {

cv::Mat glitchEffect(const cv::Mat& input, const GlitchParams& params) {
    if (input.empty()) return input;

    cv::Mat result = input.clone();
    std::mt19937 rng(params.seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_int_distribution<int> distRow(0, input.rows - 1);

    int blockH = std::max(1, static_cast<int>(params.blockSize));

    // 1. Block displacement: randomly shift horizontal strips
    int numBlocks = static_cast<int>(params.intensity * 20.0f);
    for (int b = 0; b < numBlocks; ++b) {
        int startRow = distRow(rng);
        int height = std::min(blockH, input.rows - startRow);
        int shift = static_cast<int>((dist01(rng) - 0.5f) * params.intensity * input.cols * 0.1f);

        if (shift == 0 || height <= 0) continue;

        for (int y = startRow; y < startRow + height && y < input.rows; ++y) {
            cv::Mat row = result.row(y).clone();
            for (int x = 0; x < input.cols; ++x) {
                int srcX = x - shift;
                if (srcX < 0) srcX += input.cols;
                if (srcX >= input.cols) srcX -= input.cols;

                if (input.depth() == CV_32F) {
                    int ch = input.channels();
                    for (int c = 0; c < ch; ++c) {
                        result.ptr<float>(y)[x * ch + c] = row.ptr<float>(0)[srcX * ch + c];
                    }
                } else {
                    int ch = input.channels();
                    for (int c = 0; c < ch; ++c) {
                        result.ptr<uchar>(y)[x * ch + c] = row.ptr<uchar>(0)[srcX * ch + c];
                    }
                }
            }
        }
    }

    // 2. RGB channel shift
    if (params.rgbShift > 0.0f && input.channels() >= 3) {
        result = rgbShiftEffect(result, params.rgbShift * params.intensity, 0.0f);
    }

    // 3. Scanline overlay
    if (params.scanlines > 0.0f) {
        for (int y = 0; y < result.rows; y += 2) {
            cv::Mat row = result.row(y);
            row *= (1.0f - params.scanlines * 0.5f);
        }
    }

    // 4. Random noise
    if (params.noise > 0.0f) {
        cv::Mat noise(result.size(), result.type());
        cv::randn(noise, 0.0, params.noise * (result.depth() == CV_32F ? 0.2 : 30.0));
        result += noise;
    }

    return result;
}

cv::Mat rgbShiftEffect(const cv::Mat& input, float shiftX, float shiftY) {
    if (input.empty() || input.channels() < 3) return input;

    std::vector<cv::Mat> channels;
    cv::split(input, channels);

    // Shift Red channel right, Blue channel left
    cv::Mat M_red = (cv::Mat_<float>(2, 3) << 1, 0, shiftX, 0, 1, shiftY);
    cv::Mat M_blue = (cv::Mat_<float>(2, 3) << 1, 0, -shiftX, 0, 1, -shiftY);

    cv::warpAffine(channels[2], channels[2], M_red, input.size(),
                   cv::INTER_LINEAR, cv::BORDER_WRAP);
    cv::warpAffine(channels[0], channels[0], M_blue, input.size(),
                   cv::INTER_LINEAR, cv::BORDER_WRAP);
    // Green stays centered

    cv::Mat result;
    cv::merge(channels, result);
    return result;
}

} // namespace ArtifactCore
