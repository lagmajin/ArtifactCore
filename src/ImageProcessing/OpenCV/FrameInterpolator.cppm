module;
#include <QImage>
#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp>
#include <cmath>

module OpenCV.FrameInterpolator;
import CvUtils;

namespace ArtifactCore {

QImage FrameInterpolator::interpolateOpticalFlow(const QImage& frame1, const QImage& frame2, float t) {
    if (frame1.isNull() || frame2.isNull()) return QImage();
    if (t <= 0.0f) return frame1;
    if (t >= 1.0f) return frame2;
    if (frame1.size() != frame2.size()) return frame1;

    cv::Mat mat1 = CvUtils::qImageToCvMat(frame1, true);
    cv::Mat mat2 = CvUtils::qImageToCvMat(frame2, true);

    cv::Mat gray1, gray2;
    cv::cvtColor(mat1, gray1, cv::COLOR_BGRA2GRAY);
    cv::cvtColor(mat2, gray2, cv::COLOR_BGRA2GRAY);

    cv::Mat flow;
    cv::calcOpticalFlowFarneback(gray1, gray2, flow, 0.5, 3, 15, 3, 5, 1.2, 0);

    cv::Mat mapX(flow.size(), CV_32FC1);
    cv::Mat mapY(flow.size(), CV_32FC1);

    for (int y = 0; y < flow.rows; ++y) {
        for (int x = 0; x < flow.cols; ++x) {
            cv::Point2f f = flow.at<cv::Point2f>(y, x);
            mapX.at<float>(y, x) = static_cast<float>(x) + f.x * t;
            mapY.at<float>(y, x) = static_cast<float>(y) + f.y * t;
        }
    }

    cv::Mat warped1;
    cv::remap(mat1, warped1, mapX, mapY, cv::INTER_LINEAR);

    // Simple blending between mapped frame1 and frame2
    cv::Mat blended;
    cv::addWeighted(warped1, 1.0 - t, mat2, t, 0.0, blended);

    return CvUtils::cvMatToQImage(blended);
}

QImage FrameInterpolator::applyMotionBlur(const QImage& frame, float velocityX, float velocityY, float intensity) {
    if (frame.isNull()) return QImage();
    
    cv::Mat mat = CvUtils::qImageToCvMat(frame, true);
    
    float length = std::sqrt(velocityX * velocityX + velocityY * velocityY) * intensity;
    if (length < 1.0f) return frame;
    
    int blurSize = static_cast<int>(length);
    if (blurSize % 2 == 0) blurSize++; 

    // カーネル生成
    cv::Mat kernel = cv::Mat::zeros(blurSize, blurSize, CV_32F);
    cv::line(kernel, cv::Point(0, blurSize/2), cv::Point(blurSize-1, blurSize/2), cv::Scalar(1.0), 1, cv::LINE_AA);
    
    cv::Mat kernelRot = cv::Mat::zeros(blurSize, blurSize, CV_32F);
    float angle = std::atan2(velocityY, velocityX) * 180.0f / 3.14159f;
    cv::Mat rot = cv::getRotationMatrix2D(cv::Point(blurSize/2, blurSize/2), angle, 1.0);
    cv::warpAffine(kernel, kernelRot, rot, cv::Size(blurSize, blurSize));
    
    double sum = cv::sum(kernelRot)[0];
    if (sum > 0.0) {
        kernelRot /= sum;
    } else {
        return frame;
    }

    cv::Mat blurred;
    cv::filter2D(mat, blurred, -1, kernelRot);

    return CvUtils::cvMatToQImage(blurred);
}

}
