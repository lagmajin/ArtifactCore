module;

#include <QString>
#include <QPointF>
#include <QVector3D>
#include <QImage>
#include <cmath>
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

void CameraTracker::setInitialFov(float fov) { impl_->fov = fov; }

void CameraTracker::addFrame(double time, const QImage& frame) {
    if (frame.isNull()) {
        return;
    }

    Impl::FrameData data;
    data.time = time;
    data.image = impl_->qimageToMat(frame);
    if (data.image.empty()) {
        return;
    }
    impl_->frames.push_back(std::move(data));
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

    const double focalLength = (width / 2.0) / std::tan(impl_->fov * 0.5 * CV_PI / 180.0);
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

        cv::Mat pts4D;
        cv::triangulatePoints(P1, P2, matchedPrev, matchedCur, pts4D);

        const int depth = pts4D.depth();
        for (int i = 0; i < pts4D.cols; ++i) {
            const double w = depth == CV_32F ? pts4D.at<float>(3, i) : pts4D.at<double>(3, i);
            if (std::abs(w) <= 1e-8) {
                continue;
            }

            const double x = depth == CV_32F ? pts4D.at<float>(0, i) : pts4D.at<double>(0, i);
            const double y = depth == CV_32F ? pts4D.at<float>(1, i) : pts4D.at<double>(1, i);
            const double z = depth == CV_32F ? pts4D.at<float>(2, i) : pts4D.at<double>(2, i);

            CameraTrackPoint pt;
            pt.id = nextFeatureId++;
            pt.position = QVector3D(static_cast<float>(x / w),
                                    static_cast<float>(y / w),
                                    static_cast<float>(z / w));
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

    result.success = solvedAnyStep && result.cameraPath.size() >= 2;
    return result;
}

} // namespace ArtifactCore::Tracking
