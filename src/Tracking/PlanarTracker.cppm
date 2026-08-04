module;
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>

module ArtifactCore.Tracking.PlanarTracker;

namespace ArtifactCore {

PlanarTrackResult PlanarTracker::track(const cv::Mat& previous,
                                       const cv::Mat& current,
                                       const std::array<cv::Point2f, 4>& referenceCorners,
                                       double maxError) const {
    PlanarTrackResult result;
    if (previous.empty() || current.empty() || previous.size() != current.size() ||
        maxError <= 0.0 || !std::isfinite(maxError)) return result;
    for (const auto& corner : referenceCorners) {
        if (!std::isfinite(corner.x) || !std::isfinite(corner.y)) return result;
    }

    cv::Mat previousGray, currentGray;
    if (previous.channels() == 1) previousGray = previous;
    else if (previous.channels() == 3)
        cv::cvtColor(previous, previousGray, cv::COLOR_BGR2GRAY);
    else if (previous.channels() == 4)
        cv::cvtColor(previous, previousGray, cv::COLOR_BGRA2GRAY);
    else return result;
    if (current.channels() == 1) currentGray = current;
    else if (current.channels() == 3)
        cv::cvtColor(current, currentGray, cv::COLOR_BGR2GRAY);
    else if (current.channels() == 4)
        cv::cvtColor(current, currentGray, cv::COLOR_BGRA2GRAY);
    else return result;

    std::vector<cv::Point2f> features;
    cv::goodFeaturesToTrack(previousGray, features, 500, 0.01, 5.0);
    if (features.size() < 4) return result;
    std::vector<cv::Point2f> tracked;
    std::vector<unsigned char> status;
    std::vector<float> errors;
    cv::calcOpticalFlowPyrLK(previousGray, currentGray, features, tracked, status, errors);
    if (tracked.size() != features.size() || status.size() != features.size() ||
        errors.size() != features.size()) return result;
    std::vector<cv::Point2f> from, to;
    for (size_t i = 0; i < features.size(); ++i) {
        if (status[i] && std::isfinite(tracked[i].x) &&
            std::isfinite(tracked[i].y) && std::isfinite(errors[i]) &&
            errors[i] <= maxError * 4.0f) {
            from.push_back(features[i]);
            to.push_back(tracked[i]);
        }
    }
    if (from.size() < 4) return result;

    cv::Mat inliers;
    result.homography = cv::findHomography(from, to, cv::RANSAC, maxError, inliers);
    if (result.homography.empty()) return result;
    if (result.homography.rows != 3 || result.homography.cols != 3) {
        result.homography.release();
        return result;
    }
    if (result.homography.type() != CV_64F) {
        cv::Mat homography64;
        result.homography.convertTo(homography64, CV_64F);
        result.homography = homography64;
    }
    for (int row = 0; row < result.homography.rows; ++row) {
        for (int col = 0; col < result.homography.cols; ++col) {
            if (!std::isfinite(result.homography.at<double>(row, col))) {
                result.homography.release();
                return result;
            }
        }
    }
    std::vector<cv::Point2f> source(referenceCorners.begin(), referenceCorners.end());
    std::vector<cv::Point2f> projected;
    cv::perspectiveTransform(source, projected, result.homography);
    if (projected.size() != referenceCorners.size()) {
        result.homography.release();
        return result;
    }
    for (const auto& point : projected) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            result.homography.release();
            return result;
        }
    }
    std::copy(projected.begin(), projected.end(), result.corners.begin());
    result.inlierCount = inliers.empty() ? 0 : cv::countNonZero(inliers);
    result.confidence = std::clamp(static_cast<double>(result.inlierCount) /
                                       static_cast<double>(from.size()), 0.0, 1.0);
    result.valid = result.inlierCount >= 4 && result.confidence >= 0.15;
    return result;
}

} // namespace ArtifactCore
