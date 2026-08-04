module;

#include <QString>
#include <QPointF>
#include <QVector3D>
#include <QImage>
#include <cmath>
#include <algorithm>
#include <vector>
#include <opencv2/opencv.hpp>

export module Tracking.CameraTracker;

import Transform._3D;

namespace ArtifactCore::Tracking {

export struct CameraTrackPoint {
    int id;
    QVector3D position; // 3D space
    bool isValid = false;
};

export struct CameraPose {
    double time;
    QVector3D position;
    QVector3D rotation; // Euler angles in degrees
};

export struct CameraTrackResult {
    std::vector<CameraPose> cameraPath;
    std::vector<CameraTrackPoint> featurePoints;
    bool success = false;
};

export class CameraTracker {
public:
    CameraTracker();
    ~CameraTracker();

    // カメラの初期画角（度）を設定
    void setInitialFov(float fov);

    // フレームを追加して解析
    void addFrame(double time, const QImage& frame);

    // 解析実行
    CameraTrackResult solve();

private:
    struct Impl;
    Impl* impl_;
};

} // namespace ArtifactCore::Tracking

// ============================================================================
// Implementation
// ============================================================================

namespace ArtifactCore::Tracking {

namespace {
cv::Mat makeProjectionMatrix(const cv::Mat& K, const cv::Matx33d& Rcw, const cv::Vec3d& tcw)
{
    cv::Mat Rt = cv::Mat::zeros(3, 4, CV_64F);
    cv::Mat(Rcw).copyTo(Rt(cv::Rect(0, 0, 3, 3)));
    cv::Mat tMat = (cv::Mat_<double>(3, 1) << tcw[0], tcw[1], tcw[2]);
    tMat.copyTo(Rt(cv::Rect(3, 0, 1, 3)));
    return K * Rt;
}

cv::Matx33d matToMatx33d(const cv::Mat& m)
{
    cv::Matx33d out = cv::Matx33d::eye();
    if (m.rows >= 3 && m.cols >= 3) {
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                out(row, col) = m.at<double>(row, col);
            }
        }
    }
    return out;
}

cv::Vec3d matToVec3d(const cv::Mat& m)
{
    return {
        m.at<double>(0, 0),
        m.at<double>(1, 0),
        m.at<double>(2, 0),
    };
}

QVector3D eulerDegreesFromRotation(const cv::Matx33d& rotation)
{
    const double sy = std::sqrt(rotation(0, 0) * rotation(0, 0) + rotation(1, 0) * rotation(1, 0));
    const bool singular = sy < 1e-6;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    if (!singular) {
        x = std::atan2(rotation(2, 1), rotation(2, 2));
        y = std::atan2(-rotation(2, 0), sy);
        z = std::atan2(rotation(1, 0), rotation(0, 0));
    } else {
        x = std::atan2(-rotation(1, 2), rotation(1, 1));
        y = std::atan2(-rotation(2, 0), sy);
    }

    return QVector3D(static_cast<float>(x * 180.0 / CV_PI),
                     static_cast<float>(y * 180.0 / CV_PI),
                     static_cast<float>(z * 180.0 / CV_PI));
}
} // namespace

struct CameraTracker::Impl {
    struct FrameData {
        double time;
        cv::Mat image;
    };

    std::vector<FrameData> frames;
    float fov = 45.0f;

    cv::Mat qimageToMat(const QImage& img) {
        QImage swapped = img.convertToFormat(QImage::Format_Grayscale8);
        return cv::Mat(swapped.height(), swapped.width(), CV_8UC1,
                       const_cast<uchar*>(swapped.bits()),
                       swapped.bytesPerLine()).clone();
    }
};

CameraTracker::CameraTracker() : impl_(new Impl()) {}
CameraTracker::~CameraTracker() { delete impl_; }

void CameraTracker::setInitialFov(float fov) {
    impl_->fov = std::isfinite(fov) ? std::clamp(fov, 1.0f, 179.0f) : 45.0f;
}

void CameraTracker::addFrame(double time, const QImage& frame) {
    if (!std::isfinite(time) || frame.isNull()) {
        return;
    }

    Impl::FrameData data;
    data.time = time;
    data.image = impl_->qimageToMat(frame);
    if (data.image.empty()) {
        return;
    }
    auto insertAt = std::lower_bound(
        impl_->frames.begin(), impl_->frames.end(), data.time,
        [](const Impl::FrameData& existing, double value) {
            return existing.time < value;
        });
    if (insertAt != impl_->frames.end() &&
        std::abs(insertAt->time - data.time) < 1.0e-9) {
        *insertAt = std::move(data);
    } else {
        impl_->frames.insert(insertAt, std::move(data));
    }
}

CameraTrackResult CameraTracker::solve() {
    CameraTrackResult result;
    if (impl_->frames.size() < 2) {
        return result;
    }

    const int width = impl_->frames[0].image.cols;
    const int height = impl_->frames[0].image.rows;
    if (width <= 0 || height <= 0) {
        return result;
    }

    const double fovRadians = std::clamp(static_cast<double>(impl_->fov), 1.0, 179.0) *
                              0.5 * CV_PI / 180.0;
    const double tangent = std::tan(fovRadians);
    if (!std::isfinite(tangent) || std::abs(tangent) < 1.0e-9) {
        return result;
    }
    const double focalLength = (width / 2.0) / tangent;
    const cv::Point2d principalPoint(width / 2.0, height / 2.0);
    const cv::Mat K = (cv::Mat_<double>(3, 3) << focalLength, 0.0, principalPoint.x,
                                                  0.0, focalLength, principalPoint.y,
                                                  0.0, 0.0, 1.0);

    cv::Matx33d currentRcw = cv::Matx33d::eye();
    cv::Vec3d currentTcw(0.0, 0.0, 0.0);

    result.cameraPath.push_back({
        impl_->frames[0].time,
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 0.0f, 0.0f),
    });

    int nextFeatureId = 0;
    bool solvedAnyStep = false;

    for (size_t frameIndex = 1; frameIndex < impl_->frames.size(); ++frameIndex) {
        const auto& prevFrame = impl_->frames[frameIndex - 1];
        const auto& curFrame = impl_->frames[frameIndex];
        if (prevFrame.image.size() != curFrame.image.size()) {
            continue;
        }

        std::vector<cv::Point2f> prevPoints;
        cv::goodFeaturesToTrack(prevFrame.image, prevPoints, 1000, 0.01, 10.0);

        if (prevPoints.size() < 8) {
            const cv::Vec3d worldPosition = -(currentRcw.t() * currentTcw);
            result.cameraPath.push_back({
                curFrame.time,
                QVector3D(static_cast<float>(worldPosition[0]),
                          static_cast<float>(worldPosition[1]),
                          static_cast<float>(worldPosition[2])),
                eulerDegreesFromRotation(currentRcw.t()),
            });
            continue;
        }

        std::vector<cv::Point2f> curPoints;
        std::vector<uchar> status;
        std::vector<float> err;
        cv::calcOpticalFlowPyrLK(prevFrame.image, curFrame.image, prevPoints, curPoints, status, err);

        std::vector<cv::Point2f> matchedPrev;
        std::vector<cv::Point2f> matchedCur;
        matchedPrev.reserve(prevPoints.size());
        matchedCur.reserve(prevPoints.size());
        for (size_t i = 0; i < status.size(); ++i) {
            if (status[i]) {
                matchedPrev.push_back(prevPoints[i]);
                matchedCur.push_back(curPoints[i]);
            }
        }

        if (matchedPrev.size() < 8) {
            const cv::Vec3d worldPosition = -(currentRcw.t() * currentTcw);
            result.cameraPath.push_back({
                curFrame.time,
                QVector3D(static_cast<float>(worldPosition[0]),
                          static_cast<float>(worldPosition[1]),
                          static_cast<float>(worldPosition[2])),
                eulerDegreesFromRotation(currentRcw.t()),
            });
            continue;
        }

        cv::Mat inlierMask;
        cv::Mat E = cv::findEssentialMat(matchedPrev, matchedCur, K, cv::RANSAC, 0.999, 1.0, inlierMask);
        if (E.empty()) {
            const cv::Vec3d worldPosition = -(currentRcw.t() * currentTcw);
            result.cameraPath.push_back({
                curFrame.time,
                QVector3D(static_cast<float>(worldPosition[0]),
                          static_cast<float>(worldPosition[1]),
                          static_cast<float>(worldPosition[2])),
                eulerDegreesFromRotation(currentRcw.t()),
            });
            continue;
        }

        cv::Mat R, t;
        const int poseInliers = cv::recoverPose(E, matchedPrev, matchedCur, K, R, t, inlierMask);
        if (poseInliers < 8) {
            const cv::Vec3d worldPosition = -(currentRcw.t() * currentTcw);
            result.cameraPath.push_back({
                curFrame.time,
                QVector3D(static_cast<float>(worldPosition[0]),
                          static_cast<float>(worldPosition[1]),
                          static_cast<float>(worldPosition[2])),
                eulerDegreesFromRotation(currentRcw.t()),
            });
            continue;
        }

        cv::Matx33d relativeR = matToMatx33d(R);
        cv::Vec3d relativeT = matToVec3d(t);
        const double tNorm = cv::norm(relativeT);
        if (tNorm > 1e-8) {
            relativeT *= (1.0 / tNorm);
        } else {
            relativeT = cv::Vec3d(0.0, 0.0, 1.0);
        }

        const cv::Mat P1 = makeProjectionMatrix(K, currentRcw, currentTcw);
        currentRcw = relativeR * currentRcw;
        currentTcw = relativeR * currentTcw + relativeT;
        const cv::Mat P2 = makeProjectionMatrix(K, currentRcw, currentTcw);

        std::vector<cv::Point2f> inlierPrev;
        std::vector<cv::Point2f> inlierCur;
        inlierPrev.reserve(matchedPrev.size());
        inlierCur.reserve(matchedCur.size());
        const bool hasInlierMask = !inlierMask.empty() &&
                                   inlierMask.total() >= matchedPrev.size();
        for (int index = 0; index < static_cast<int>(matchedPrev.size()); ++index) {
            const uchar inlier = hasInlierMask
                ? inlierMask.ptr<uchar>(0)[index] : 0;
            if (!hasInlierMask || inlier == 0) {
                continue;
            }
            inlierPrev.push_back(matchedPrev[static_cast<std::size_t>(index)]);
            inlierCur.push_back(matchedCur[static_cast<std::size_t>(index)]);
        }
        if (inlierPrev.size() < 4) {
            continue;
        }

        cv::Mat pts4D;
        cv::triangulatePoints(P1, P2, inlierPrev, inlierCur, pts4D);

        const int depth = pts4D.depth();
        for (int i = 0; i < pts4D.cols; ++i) {
            const double w = depth == CV_32F ? pts4D.at<float>(3, i) : pts4D.at<double>(3, i);
            if (!std::isfinite(w) || std::abs(w) <= 1e-8) {
                continue;
            }

            const double x = depth == CV_32F ? pts4D.at<float>(0, i) : pts4D.at<double>(0, i);
            const double y = depth == CV_32F ? pts4D.at<float>(1, i) : pts4D.at<double>(1, i);
            const double z = depth == CV_32F ? pts4D.at<float>(2, i) : pts4D.at<double>(2, i);

            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
                continue;
            }
            const double worldX = x / w;
            const double worldY = y / w;
            const double worldZ = z / w;
            if (!std::isfinite(worldX) || !std::isfinite(worldY) ||
                !std::isfinite(worldZ)) {
                continue;
            }
            CameraTrackPoint pt;
            pt.id = nextFeatureId++;
            pt.position = QVector3D(static_cast<float>(worldX),
                                    static_cast<float>(worldY),
                                    static_cast<float>(worldZ));
            pt.isValid = true;
            result.featurePoints.push_back(pt);
        }

        const cv::Vec3d worldPosition = -(currentRcw.t() * currentTcw);
        result.cameraPath.push_back({
            curFrame.time,
            QVector3D(static_cast<float>(worldPosition[0]),
                      static_cast<float>(worldPosition[1]),
                      static_cast<float>(worldPosition[2])),
            eulerDegreesFromRotation(currentRcw.t()),
        });

        solvedAnyStep = true;
    }

    result.cameraPath.erase(
        std::remove_if(result.cameraPath.begin(), result.cameraPath.end(),
            [](const CameraPose& pose) {
                const auto finiteVector = [](const QVector3D& value) {
                    return std::isfinite(value.x()) && std::isfinite(value.y()) &&
                           std::isfinite(value.z());
                };
                return !std::isfinite(pose.time) ||
                       !finiteVector(pose.position) ||
                       !finiteVector(pose.rotation);
            }),
        result.cameraPath.end());
    result.featurePoints.erase(
        std::remove_if(result.featurePoints.begin(), result.featurePoints.end(),
            [](const CameraTrackPoint& point) {
                return !std::isfinite(point.position.x()) ||
                       !std::isfinite(point.position.y()) ||
                       !std::isfinite(point.position.z());
            }),
        result.featurePoints.end());
    result.success = solvedAnyStep && result.cameraPath.size() >= 2 &&
                     !result.featurePoints.empty();
    return result;
}

} // namespace ArtifactCore::Tracking
