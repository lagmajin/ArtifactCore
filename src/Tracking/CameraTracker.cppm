module;

#include <QString>
#include <QPointF>
#include <QVector3D>
#include <QImage>
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

struct CameraTracker::Impl {
    struct FrameData {
        double time;
        cv::Mat image;
        std::vector<cv::Point2f> keypoints;
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
    Impl::FrameData data;
    data.time = time;
    data.image = impl_->qimageToMat(frame);
    impl_->frames.push_back(std::move(data));
}

CameraTrackResult CameraTracker::solve() {
    CameraTrackResult result;
    if (impl_->frames.size() < 2) return result;

    const int width = impl_->frames[0].image.cols;
    const int height = impl_->frames[0].image.rows;

    // 1. カメラマトリックスの推定 (簡易)
    double focalLength = (width / 2.0) / std::tan(impl_->fov * 0.5 * CV_PI / 180.0);
    cv::Point2d principalPoint(width / 2.0, height / 2.0);
    cv::Mat K = (cv::Mat_<double>(3,3) << focalLength, 0, principalPoint.x, 
                                          0, focalLength, principalPoint.y,
                                          0, 0, 1);

    // 2. 特徴点抽出と追跡 (第一段階: シンプルな2フレーム間ポーズ推定)
    // 本来は全フレームを通したSfMが必要だが、ここでは初期ポーズ推定のみ実装
    std::vector<cv::Point2f> pts1, pts2;
    cv::goodFeaturesToTrack(impl_->frames[0].image, pts1, 500, 0.01, 10);
    
    std::vector<uchar> status;
    std::vector<float> err;
    cv::calcOpticalFlowPyrLK(impl_->frames[0].image, impl_->frames[1].image, pts1, pts2, status, err);

    std::vector<cv::Point2f> matched1, matched2;
    for (size_t i = 0; i < status.size(); i++) {
        if (status[i]) {
            matched1.push_back(pts1[i]);
            matched2.push_back(pts2[i]);
        }
    }

    if (matched1.size() < 10) return result;

    // 3. エッセンシャル行列の計算
    cv::Mat E, mask;
    E = cv::findEssentialMat(matched1, matched2, focalLength, principalPoint, cv::RANSAC, 0.999, 1.0, mask);

    // 4. ポーズ復元
    cv::Mat R, t;
    cv::recoverPose(E, matched1, matched2, R, t, focalLength, principalPoint, mask);

    // 回転行列をオイラー角（度）に変換
    auto getEulerAngles = [](const cv::Mat& R) {
        double sy = std::sqrt(R.at<double>(0,0) * R.at<double>(0,0) +  R.at<double>(1,0) * R.at<double>(1,0));
        bool singular = sy < 1e-6;
        double x, y, z;
        if (!singular) {
            x = std::atan2(R.at<double>(2,1) , R.at<double>(2,2));
            y = std::atan2(-R.at<double>(2,0), sy);
            z = std::atan2(R.at<double>(1,0), R.at<double>(0,0));
        } else {
            x = std::atan2(-R.at<double>(1,2), R.at<double>(1,1));
            y = std::atan2(-R.at<double>(2,0), sy);
            z = 0;
        }
        return QVector3D(x * 180.0 / CV_PI, y * 180.0 / CV_PI, z * 180.0 / CV_PI);
    };

    QVector3D rot0(0,0,0);
    QVector3D rot1 = getEulerAngles(R);

    // 結果の格納
    result.cameraPath.push_back({impl_->frames[0].time, QVector3D(0,0,0), rot0});
    result.cameraPath.push_back({impl_->frames[1].time, 
                                 QVector3D(t.at<double>(0), t.at<double>(1), t.at<double>(2)), 
                                 rot1});

    // 5. 三角測量による3Dポイント生成
    cv::Mat P1 = cv::Mat::eye(3, 4, CV_64F);
    cv::Mat P2(3, 4, CV_64F);
    R.copyTo(P2(cv::Rect(0,0,3,3)));
    t.copyTo(P2(cv::Rect(3,0,1,3)));

    // 正規化座標
    std::vector<cv::Point2f> pts1_norm, pts2_norm;
    cv::undistortPoints(matched1, pts1_norm, K, cv::Mat());
    cv::undistortPoints(matched2, pts2_norm, K, cv::Mat());

    cv::Mat pts4D;
    cv::triangulatePoints(P1, P2, pts1_norm, pts2_norm, pts4D);

    for (int i = 0; i < pts4D.cols; i++) {
        float w = pts4D.at<float>(3, i);
        if (std::abs(w) > 1e-6) {
            CameraTrackPoint pt;
            pt.id = i;
            pt.position = QVector3D(pts4D.at<float>(0, i) / w,
                                    pts4D.at<float>(1, i) / w,
                                    pts4D.at<float>(2, i) / w);
            pt.isValid = true;
            result.featurePoints.push_back(pt);
        }
    }

    result.success = true;
    return result;
}

} // namespace ArtifactCore::Tracking
