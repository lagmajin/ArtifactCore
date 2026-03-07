module;
#include <opencv2/opencv.hpp>
#include <cmath>
module Image:ImageProcessing;

namespace ArtifactCore {

    struct HalftoneParams { float dotSize; float angle; float contrast; bool colorMode; float cmykAngles[4]; };

cv::Mat halftoneEffect(const cv::Mat& input, const HalftoneParams& params) {
    if (input.empty()) return input;

    // Convert to float for processing
    cv::Mat src;
    if (input.depth() != CV_32F) {
        input.convertTo(src, CV_32F, 1.0 / 255.0);
    } else {
        src = input.clone();
    }

    cv::Mat result = cv::Mat::ones(src.size(), src.type());
    float dotRadius = params.dotSize * 0.5f;
    int step = std::max<int>(1, static_cast<int>(params.dotSize));

    if (!params.colorMode || src.channels() < 3) {
        // Monochrome halftone
        cv::Mat gray;
        if (src.channels() >= 3) {
            cv::cvtColor(src, gray, (src.channels() == 4) ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
        } else {
            gray = src;
        }

        cv::Mat out = cv::Mat::ones(gray.size(), CV_32F);
        float angleRad = params.angle * static_cast<float>(CV_PI) / 180.0f;
        float cosA = std::cos(angleRad);
        float sinA = std::sin(angleRad);

        for (int cy = 0; cy < gray.rows; cy += step) {
            for (int cx = 0; cx < gray.cols; cx += step) {
                // Sample luminance at grid center
                float lum = gray.at<float>(
                    std::min(cy + step / 2, gray.rows - 1),
                    std::min(cx + step / 2, gray.cols - 1));

                float radius = (1.0f - lum) * dotRadius * params.contrast;

                // Draw dot
                cv::circle(out,
                    cv::Point(cx + step / 2, cy + step / 2),
                    std::max(0, static_cast<int>(radius)),
                    cv::Scalar(0.0f), -1, cv::LINE_AA);
            }
        }

        // Convert back to input channel count
        if (src.channels() == 4) {
            std::vector<cv::Mat> ch(4);
            cv::split(src, ch);
            cv::merge(std::vector<cv::Mat>{out, out, out, ch[3]}, result);
        } else if (src.channels() == 3) {
            cv::merge(std::vector<cv::Mat>{out, out, out}, result);
        } else {
            result = out;
        }
    } else {
        // Color CMYK halftone
        std::vector<cv::Mat> channels;
        cv::Mat bgr;
        if (src.channels() == 4) {
            cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
        } else {
            bgr = src;
        }

        // Convert to CMY (simplified)
        std::vector<cv::Mat> bgrCh(3);
        cv::split(bgr, bgrCh);

        cv::Mat cmyChannels[3];
        for (int i = 0; i < 3; ++i) {
            cmyChannels[i] = 1.0f - bgrCh[i]; // Invert: BGR -> CMY
        }

        // Process each CMY channel with different screen angle
        cv::Mat outChannels[3];
        for (int c = 0; c < 3; ++c) {
            outChannels[c] = cv::Mat::zeros(src.size(), CV_32F);
            for (int cy = 0; cy < src.rows; cy += step) {
                for (int cx = 0; cx < src.cols; cx += step) {
                    float val = cmyChannels[c].at<float>(
                        std::min(cy + step / 2, src.rows - 1),
                        std::min(cx + step / 2, src.cols - 1));

                    float radius = val * dotRadius * params.contrast;

                    cv::circle(outChannels[c],
                        cv::Point(cx + step / 2, cy + step / 2),
                        std::max(0, static_cast<int>(radius)),
                        cv::Scalar(1.0f), -1, cv::LINE_AA);
                }
            }
            // Invert back: CMY -> BGR
            outChannels[c] = 1.0f - outChannels[c];
        }

        cv::merge(std::vector<cv::Mat>{outChannels[0], outChannels[1], outChannels[2]}, result);

        if (src.channels() == 4) {
            std::vector<cv::Mat> srcCh(4);
            cv::split(src, srcCh);
            std::vector<cv::Mat> resCh(3);
            cv::split(result, resCh);
            resCh.push_back(srcCh[3]);
            cv::merge(resCh, result);
        }
    }

    // Convert back to original depth
    if (input.depth() != CV_32F) {
        result.convertTo(result, input.depth(), 255.0);
    }

    return result;
}

cv::Mat orderedDither(const cv::Mat& input, int matrixSize) {
    if (input.empty()) return input;

    // Bayer 4x4 dithering matrix
    const float bayer4x4[4][4] = {
        { 0.0f/16, 8.0f/16, 2.0f/16, 10.0f/16},
        {12.0f/16, 4.0f/16, 14.0f/16, 6.0f/16},
        { 3.0f/16, 11.0f/16, 1.0f/16, 9.0f/16},
        {15.0f/16, 7.0f/16, 13.0f/16, 5.0f/16}
    };

    cv::Mat src;
    if (input.depth() != CV_32F) {
        input.convertTo(src, CV_32F, 1.0 / 255.0);
    } else {
        src = input.clone();
    }

    cv::Mat gray;
    if (src.channels() >= 3) {
        cv::cvtColor(src, gray, (src.channels() == 4) ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
    } else {
        gray = src;
    }

    cv::Mat result = cv::Mat::zeros(gray.size(), CV_32F);
    for (int y = 0; y < gray.rows; ++y) {
        for (int x = 0; x < gray.cols; ++x) {
            float val = gray.at<float>(y, x);
            float threshold = bayer4x4[y % 4][x % 4];
            result.at<float>(y, x) = (val > threshold) ? 1.0f : 0.0f;
        }
    }

    if (input.depth() != CV_32F) {
        result.convertTo(result, input.depth(), 255.0);
    }

    return result;
}

} // namespace ArtifactCore
