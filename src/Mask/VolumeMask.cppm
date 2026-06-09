module;
#include <vector>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
module Core.Mask.VolumeMask;

namespace ArtifactCore {

cv::Mat VolumeMaskGenerator::generateDistanceField(
    const std::vector<cv::Point>& contour,
    int width, int height)
{
    // バイナリマスクをフィル
    cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);
    std::vector<std::vector<cv::Point>> contours = { contour };
    cv::fillPoly(mask, contours, cv::Scalar(255));

    // 内部距離（輪郭から内側への距離）
    cv::Mat distInside;
    cv::distanceTransform(mask, distInside, cv::DIST_L2, cv::DIST_MASK_PRECISE);

    // 外部距離（輪郭から外側への距離）
    cv::Mat distOutside;
    cv::Mat invertedMask = 255 - mask;
    cv::distanceTransform(invertedMask, distOutside, cv::DIST_L2, cv::DIST_MASK_PRECISE);

    // SDF: 内部正、外部負
    cv::Mat sdf = cv::Mat::zeros(height, width, CV_32FC1);
    cv::subtract(distInside, distOutside, sdf);

    return sdf;
}

cv::Mat VolumeMaskGenerator::computeAlphaMask(
    const cv::Mat& distanceField,
    const VolumeMaskSettings& settings)
{
    const int h = distanceField.rows;
    const int w = distanceField.cols;
    cv::Mat alpha(h, w, CV_32FC1);

    const float thickness = std::max(0.001f, settings.thickness);

    // 減衰カーブの関数
    auto falloff = [&](float t) -> float {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (settings.falloffCurve) {
            case VolumeFalloffCurve::Step:
                return 1.0f;
            case VolumeFalloffCurve::Linear:
                return t;
            case VolumeFalloffCurve::EaseIn:
                return t * t;
            case VolumeFalloffCurve::EaseOut:
                return 1.0f - (1.0f - t) * (1.0f - t);
        }
        return t;
    };

    for (int y = 0; y < h; ++y) {
        const float* rowDist = distanceField.ptr<float>(y);
        float* rowAlpha = alpha.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            const float dist = rowDist[x];
            float a;
            if (dist < 0.0f) {
                a = 0.0f;
            } else {
                const float t = std::min(dist / thickness, 1.0f);
                a = falloff(t) * settings.density;
            }
            rowAlpha[x] = a;
        }
    }

    if (settings.invert) {
        cv::subtract(cv::Scalar(1.0f), alpha, alpha);
    }

    return alpha;
}

void VolumeMaskGenerator::renderToAlpha(
    const std::vector<cv::Point>& contour,
    int width, int height,
    void* outMat,
    const VolumeMaskSettings& settings)
{
    cv::Mat& dst = *static_cast<cv::Mat*>(outMat);

    if (contour.size() < 3) {
        dst = cv::Mat::zeros(height, width, CV_32FC1);
        return;
    }

    const cv::Mat sdf = generateDistanceField(contour, width, height);
    dst = computeAlphaMask(sdf, settings);
}

}
