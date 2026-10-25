module;
#include <cstdint>
#include <vector>
#include <QString>
#include <opencv2/core/mat.hpp>

module Core.AI.RotoBrushImageSegmenter;

import Core.AI.ImageSegmenter;
import ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine;

namespace ArtifactCore {

class RotoBrushImageSegmenter::Impl {
public:
    OpenCVRotoBrushEngine engine;
    std::vector<RotoBrushStroke> strokes;
    QString lastErrorMessage;
};

RotoBrushImageSegmenter::RotoBrushImageSegmenter() : impl_(new Impl()) {}

RotoBrushImageSegmenter::~RotoBrushImageSegmenter()
{
    delete impl_;
    impl_ = nullptr;
}

bool RotoBrushImageSegmenter::isReady() const noexcept { return impl_ != nullptr; }

bool RotoBrushImageSegmenter::segment(
    const ImageF32x4_RGBA& source, DepthMap& foregroundMask)
{
    if (!impl_ || source.isEmpty()) {
        foregroundMask.clear();
        if (impl_) { impl_->lastErrorMessage = QStringLiteral("RotoBrush source image is empty."); }
        return false;
    }

    const cv::Mat sourceMat = source.toCanonicalBGRA32FC4();
    if (sourceMat.empty()) {
        foregroundMask.clear();
        impl_->lastErrorMessage = QStringLiteral("RotoBrush could not access canonical source pixels.");
        return false;
    }
    impl_->engine.updateBaseFrame(&sourceMat, impl_->strokes);
    const auto* mask = static_cast<const cv::Mat*>(impl_->engine.currentMask());
    if (!mask || mask->empty() || mask->type() != CV_8UC1) {
        foregroundMask.clear();
        impl_->lastErrorMessage = QStringLiteral("RotoBrush did not produce an alpha mask.");
        return false;
    }

    foregroundMask.resize(mask->cols, mask->rows);
    for (int y = 0; y < mask->rows; ++y) {
        const auto* row = mask->ptr<std::uint8_t>(y);
        for (int x = 0; x < mask->cols; ++x) {
            foregroundMask.setValue(x, y, static_cast<float>(row[x]) / 255.0f);
        }
    }
    impl_->lastErrorMessage.clear();
    return true;
}

bool RotoBrushImageSegmenter::propagateToNextFrame(
    const ImageF32x4_RGBA& previousFrame,
    const ImageF32x4_RGBA& currentFrame,
    DepthMap& foregroundMask)
{
    if (!impl_ || previousFrame.isEmpty() || currentFrame.isEmpty()) {
        foregroundMask.clear();
        if (impl_) { impl_->lastErrorMessage = QStringLiteral("RotoBrush propagation requires two non-empty frames."); }
        return false;
    }
    const cv::Mat previousMat = previousFrame.toCanonicalBGRA32FC4();
    const cv::Mat currentMat = currentFrame.toCanonicalBGRA32FC4();
    if (previousMat.empty() || currentMat.empty() || previousMat.size() != currentMat.size()) {
        foregroundMask.clear();
        impl_->lastErrorMessage = QStringLiteral("RotoBrush propagation frames must have matching canonical dimensions.");
        return false;
    }
    impl_->engine.propagateToNextFrame(&previousMat, &currentMat);
    const auto* mask = static_cast<const cv::Mat*>(impl_->engine.currentMask());
    if (!mask || mask->empty() || mask->type() != CV_8UC1) {
        foregroundMask.clear();
        impl_->lastErrorMessage = QStringLiteral("RotoBrush propagation did not produce an alpha mask.");
        return false;
    }
    foregroundMask.resize(mask->cols, mask->rows);
    for (int y = 0; y < mask->rows; ++y) {
        const auto* row = mask->ptr<std::uint8_t>(y);
        for (int x = 0; x < mask->cols; ++x) {
            foregroundMask.setValue(x, y, static_cast<float>(row[x]) / 255.0f);
        }
    }
    impl_->lastErrorMessage.clear();
    return true;
}

QString RotoBrushImageSegmenter::lastError() const
{
    return impl_ ? impl_->lastErrorMessage : QStringLiteral("RotoBrush segmenter is unavailable.");
}

void RotoBrushImageSegmenter::setStrokes(const std::vector<RotoBrushStroke>& strokes)
{
    impl_->strokes = strokes;
}

const std::vector<RotoBrushStroke>& RotoBrushImageSegmenter::strokes() const noexcept
{
    return impl_->strokes;
}

void RotoBrushImageSegmenter::clearStrokes()
{
    impl_->strokes.clear();
    impl_->engine.reset();
}

} // namespace ArtifactCore
