module;
#include <utility>
#include <random>
#include <cmath>
#include <opencv2/opencv.hpp>
module VHS_CV;

namespace ArtifactCore {

cv::Mat vhsEffect(const cv::Mat& input, const VHSParams& params) {
    if (input.empty()) return input;

    cv::Mat src;
    if (input.depth() != CV_32F) {
        input.convertTo(src, CV_32F, 1.0 / 255.0);
    } else {
        src = input.clone();
    }

    cv::Mat result = src.clone();
    std::mt19937 rng(params.seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    // 1. Color bleed: blur chroma channels (YCrCb space)
    if (params.colorBleed > 0.0f) {
        cv::Mat bgr;
        if (src.channels() == 4) {
            cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
        } else {
            bgr = src;
        }

        cv::Mat ycrcb;
        bgr.convertTo(bgr, CV_8UC3, 255.0);
        cv::cvtColor(bgr, ycrcb, cv::COLOR_BGR2YCrCb);

        std::vector<cv::Mat> ycrcbCh;
        cv::split(ycrcb, ycrcbCh);

        // Blur chroma channels horizontally
        int blurSize = static_cast<int>(params.colorBleed * 20.0f) * 2 + 1;
        cv::GaussianBlur(ycrcbCh[1], ycrcbCh[1], cv::Size(blurSize, 1), 0);
        cv::GaussianBlur(ycrcbCh[2], ycrcbCh[2], cv::Size(blurSize, 1), 0);

        cv::merge(ycrcbCh, ycrcb);
        cv::cvtColor(ycrcb, bgr, cv::COLOR_YCrCb2BGR);
        bgr.convertTo(bgr, CV_32F, 1.0 / 255.0);

        if (src.channels() == 4) {
            std::vector<cv::Mat> srcCh(4), bgrCh(3);
            cv::split(src, srcCh);
            cv::split(bgr, bgrCh);
            bgrCh.push_back(srcCh[3]);
            cv::merge(bgrCh, result);
        } else {
            result = bgr;
        }
    }

    // 2. Desaturation
    if (params.saturation < 1.0f) {
        cv::Mat bgr3;
        if (result.channels() == 4) {
            cv::cvtColor(result, bgr3, cv::COLOR_BGRA2BGR);
        } else {
            bgr3 = result.clone();
        }
        bgr3.convertTo(bgr3, CV_8UC3, 255.0);
        cv::Mat hsv;
        cv::cvtColor(bgr3, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> hsvCh;
        cv::split(hsv, hsvCh);
        hsvCh[1] *= params.saturation;
        cv::merge(hsvCh, hsv);
        cv::cvtColor(hsv, bgr3, cv::COLOR_HSV2BGR);
        bgr3.convertTo(bgr3, CV_32F, 1.0 / 255.0);

        if (result.channels() == 4) {
            std::vector<cv::Mat> resCh(4), bgrCh(3);
            cv::split(result, resCh);
            cv::split(bgr3, bgrCh);
            bgrCh.push_back(resCh[3]);
            cv::merge(bgrCh, result);
        } else {
            result = bgr3;
        }
    }

    // 3. Horizontal line wobble (tracking error simulation)
    if (params.wobble > 0.0f || params.trackingError > 0.0f) {
        cv::Mat wobbled = result.clone();
        for (int y = 0; y < result.rows; ++y) {
            float wobbleOffset = std::sin(y * 0.03f + dist01(rng) * 6.28f) * params.wobble * 3.0f;
            float trackingOffset = 0.0f;

            // Random tracking error bands
            if (dist01(rng) < params.trackingError * 0.05f) {
                trackingOffset = (dist01(rng) - 0.5f) * params.trackingError * 30.0f;
            }

            int shift = static_cast<int>(wobbleOffset + trackingOffset);
            if (shift == 0) continue;

            for (int x = 0; x < result.cols; ++x) {
                int srcX = x - shift;
                srcX = std::max(0, std::min(result.cols - 1, srcX));

                int ch = result.channels();
                for (int c = 0; c < ch; ++c) {
                    wobbled.ptr<float>(y)[x * ch + c] = result.ptr<float>(y)[srcX * ch + c];
                }
            }
        }
        result = wobbled;
    }

    // 4. Tape noise
    if (params.noiseAmount > 0.0f) {
        cv::Mat noise(result.size(), result.type());
        cv::randn(noise, 0.0, params.noiseAmount * 0.15);
        result += noise;
    }

    // 5. Scanlines
    result = scanlineOverlay(result, params.scanlineGap, 0.2f);

    // 6. Sharpness reduction
    if (params.sharpness < 1.0f) {
        cv::Mat blurred;
        float sigma = (1.0f - params.sharpness) * 2.0f;
        cv::GaussianBlur(result, blurred, cv::Size(0, 0), sigma);
        result = result * params.sharpness + blurred * (1.0f - params.sharpness);
    }

    // Clamp
    cv::min(result, 1.0f, result);
    cv::max(result, 0.0f, result);

    if (input.depth() != CV_32F) {
        result.convertTo(result, input.depth(), 255.0);
    }

    return result;
}

cv::Mat scanlineOverlay(const cv::Mat& input, float gap, float intensity) {
    if (input.empty()) return input;

    cv::Mat result = input.clone();
    int step = std::max(1, static_cast<int>(gap));

    for (int y = 0; y < result.rows; y += step) {
        cv::Mat row = result.row(y);
        if (result.depth() == CV_32F) {
            row *= (1.0f - intensity);
        } else {
            row *= (1.0 - static_cast<double>(intensity));
        }
    }

    return result;
}

} // namespace ArtifactCore
