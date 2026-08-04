module;
#include <array>
#include <opencv2/opencv.hpp>

export module ArtifactCore.Tracking.PlanarTracker;

namespace ArtifactCore {

export struct PlanarTrackResult {
    cv::Mat homography;
    std::array<cv::Point2f, 4> corners{};
    int inlierCount = 0;
    double confidence = 0.0;
    bool valid = false;
};

export class PlanarTracker {
public:
    PlanarTrackResult track(const cv::Mat& previous,
                            const cv::Mat& current,
                            const std::array<cv::Point2f, 4>& referenceCorners,
                            double maxError = 3.0) const;
};

} // namespace ArtifactCore
