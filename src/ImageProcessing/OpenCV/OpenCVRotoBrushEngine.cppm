module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <opencv2/opencv.hpp>

module ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine;
import Core.Parallel;

namespace ArtifactCore {

class OpenCVRotoBrushEngine::Impl {
public:
    cv::Mat baseImage;
    cv::Mat currentImage;
    cv::Mat currentMask;

    static bool validImage(const cv::Mat& image) {
        return !image.empty() && image.cols > 1 && image.rows > 1 &&
               (image.depth() == CV_8U || image.depth() == CV_32F) &&
               image.channels() >= 1 && image.channels() <= 4;
    }

    static cv::Mat grabCutImage(const cv::Mat& image) {
        cv::Mat converted;
        if (image.channels() == 1) {
            cv::cvtColor(image, converted, cv::COLOR_GRAY2BGR);
        } else if (image.channels() == 4) {
            cv::cvtColor(image, converted, cv::COLOR_BGRA2BGR);
        } else {
            converted = image;
        }
        if (converted.depth() != CV_8U) {
            double minValue = 0.0;
            double maxValue = 1.0;
            cv::minMaxLoc(converted.reshape(1), &minValue, &maxValue);
            const double scale = maxValue <= 1.0 ? 255.0 : 1.0;
            converted.convertTo(converted, CV_8U, scale);
        }
        return converted;
    }

    static cv::Mat gray8(const cv::Mat& image) {
        cv::Mat gray;
        if (image.channels() == 1) {
            gray = image;
        } else if (image.channels() == 4) {
            cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
        } else {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        }
        if (gray.depth() != CV_8U) {
            double minValue = 0.0;
            double maxValue = 1.0;
            cv::minMaxLoc(gray, &minValue, &maxValue);
            const double scale = maxValue > minValue ? 255.0 / (maxValue - minValue) : 1.0;
            gray.convertTo(gray, CV_8U, scale, -minValue * scale);
        }
        return gray;
    }
};

OpenCVRotoBrushEngine::OpenCVRotoBrushEngine()
    : impl_(std::make_unique<Impl>()) {}

OpenCVRotoBrushEngine::~OpenCVRotoBrushEngine() = default;

void OpenCVRotoBrushEngine::updateBaseFrame(
    const void* sourceImagePtr,
    const std::vector<RotoBrushStroke>& strokes)
{
    if (!sourceImagePtr) {
        reset();
        return;
    }
    const auto& sourceImage = *static_cast<const cv::Mat*>(sourceImagePtr);
    if (!Impl::validImage(sourceImage)) {
        reset();
        return;
    }

    const cv::Mat workingImage = Impl::grabCutImage(sourceImage);
    impl_->baseImage = workingImage.clone();
    impl_->currentImage = impl_->baseImage.clone();

    cv::Mat grabMask(workingImage.size(), CV_8UC1, cv::Scalar(cv::GC_BGD));
    cv::Rect foregroundBounds(0, 0, workingImage.cols, workingImage.rows);
    bool hasForeground = false;
    bool hasBackground = false;

    for (const auto& stroke : strokes) {
        if (stroke.points.empty()) continue;
        std::vector<cv::Point> points;
        points.reserve(stroke.points.size());
        for (const auto& point : stroke.points) {
            if (point.x >= 0 && point.y >= 0 &&
                point.x < workingImage.cols && point.y < workingImage.rows) {
                points.emplace_back(point.x, point.y);
            }
        }
        if (points.empty()) continue;
        const int thickness = std::clamp(stroke.thickness, 1, 512);
        const int label = stroke.type == RotoBrushStrokeType::Foreground
                              ? cv::GC_FGD : cv::GC_BGD;
        if (stroke.type == RotoBrushStrokeType::Foreground) hasForeground = true;
        else hasBackground = true;
        for (std::size_t i = 1; i < points.size(); ++i) {
            cv::line(grabMask, cv::Point(points[i - 1].x, points[i - 1].y),
                     cv::Point(points[i].x, points[i].y),
                     cv::Scalar(label), thickness, cv::LINE_AA);
        }
        if (points.size() == 1) {
            cv::circle(grabMask, cv::Point(points.front().x, points.front().y), thickness / 2,
                       cv::Scalar(label), cv::FILLED, cv::LINE_AA);
        }
    }

    if (!hasForeground) {
        const int marginX = std::max(1, workingImage.cols / 10);
        const int marginY = std::max(1, workingImage.rows / 10);
        foregroundBounds = cv::Rect(marginX, marginY,
            std::max(1, workingImage.cols - 2 * marginX),
            std::max(1, workingImage.rows - 2 * marginY));
    } else {
        foregroundBounds = cv::Rect(0, 0, workingImage.cols, workingImage.rows);
    }

    cv::Mat bgdModel, fgdModel;
    try {
        const int mode = (hasForeground || hasBackground)
                           ? cv::GC_INIT_WITH_MASK : cv::GC_INIT_WITH_RECT;
        cv::grabCut(workingImage, grabMask, foregroundBounds, bgdModel, fgdModel,
                    5, mode);
        cv::Mat probableForeground;
        cv::Mat definiteForeground;
        cv::compare(grabMask, cv::GC_PR_FGD, probableForeground, cv::CMP_EQ);
        cv::compare(grabMask, cv::GC_FGD, definiteForeground, cv::CMP_EQ);
        cv::bitwise_or(probableForeground, definiteForeground, impl_->currentMask);
    } catch (const cv::Exception&) {
        impl_->currentMask = cv::Mat(workingImage.size(), CV_8UC1, cv::Scalar(0));
        for (const auto& stroke : strokes) {
            if (stroke.type != RotoBrushStrokeType::Foreground) continue;
            const int thickness = std::clamp(stroke.thickness, 1, 512);
            for (std::size_t i = 1; i < stroke.points.size(); ++i) {
                cv::line(impl_->currentMask,
                         cv::Point(stroke.points[i - 1].x, stroke.points[i - 1].y),
                         cv::Point(stroke.points[i].x, stroke.points[i].y),
                         cv::Scalar(255), thickness, cv::LINE_AA);
            }
            if (stroke.points.size() == 1) {
                cv::circle(impl_->currentMask,
                           cv::Point(stroke.points.front().x, stroke.points.front().y),
                           std::max(1, thickness / 2), cv::Scalar(255),
                           cv::FILLED, cv::LINE_AA);
            }
        }
    }
}

void OpenCVRotoBrushEngine::propagateToNextFrame(
    const void* previousImagePtr,
    const void* currentImagePtr)
{
    if (!previousImagePtr || !currentImagePtr) return;
    const auto& previousImage = *static_cast<const cv::Mat*>(previousImagePtr);
    const auto& currentImage = *static_cast<const cv::Mat*>(currentImagePtr);
    if (!Impl::validImage(previousImage) || !Impl::validImage(currentImage) ||
        impl_->currentMask.empty() || previousImage.size() != currentImage.size() ||
        previousImage.size() != impl_->currentMask.size()) {
        return;
    }

    try {
        const cv::Mat previousGray = Impl::gray8(previousImage);
        const cv::Mat currentGray = Impl::gray8(currentImage);
        cv::Mat flow;
        cv::calcOpticalFlowFarneback(previousGray, currentGray, flow,
            0.5, 3, 21, 3, 5, 1.2, 0);

        cv::Mat mapX(flow.size(), CV_32FC1);
        cv::Mat mapY(flow.size(), CV_32FC1);
        Parallel::ForTiles(flow.cols, flow.rows, 32, 32,
            [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
            const auto* flowRow = flow.ptr<cv::Point2f>(y);
            auto* xRow = mapX.ptr<float>(y);
            auto* yRow = mapY.ptr<float>(y);
            for (int x = x0; x < x1; ++x) {
                xRow[x] = static_cast<float>(x) - flowRow[x].x;
                yRow[x] = static_cast<float>(y) - flowRow[x].y;
            }
        }
        });
        cv::Mat warped;
        cv::remap(impl_->currentMask, warped, mapX, mapY,
                  cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));
        // Optical-flow interpolation creates fractional edge coverage. Keep
        // the propagated brush matte deterministic and remove isolated flow
        // speckles before exposing it to the compositor.
        cv::threshold(warped, warped, 127.0, 255.0, cv::THRESH_BINARY);
        impl_->currentMask = std::move(warped);
        refineCurrentMask(1);
        impl_->currentImage = currentImage.clone();
    } catch (const cv::Exception&) {
        // Keep the last valid mask when an optional tracking backend rejects input.
    }
}

const void* OpenCVRotoBrushEngine::currentMask() const {
    return &impl_->currentMask;
}

void OpenCVRotoBrushEngine::refineCurrentMask(int radius) {
    if (radius <= 0 || impl_->currentMask.empty()) return;
    const int safeRadius = std::clamp(radius, 1, 32);
    const int kernelSize = safeRadius * 2 + 1;
    try {
        const cv::Mat kernel = cv::getStructuringElement(
            cv::MORPH_ELLIPSE, cv::Size(kernelSize, kernelSize));
        cv::morphologyEx(impl_->currentMask, impl_->currentMask,
                         cv::MORPH_OPEN, kernel);
        cv::morphologyEx(impl_->currentMask, impl_->currentMask,
                         cv::MORPH_CLOSE, kernel);
    } catch (const cv::Exception&) {
        // Keep the previous valid mask when the optional cleanup backend fails.
    }
}

void OpenCVRotoBrushEngine::reset() {
    impl_->baseImage.release();
    impl_->currentImage.release();
    impl_->currentMask.release();
}

} // namespace ArtifactCore
