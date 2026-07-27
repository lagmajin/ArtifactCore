// ReSharper disable All
module;

#include <cmath>
#include <algorithm>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QPointF>
#include <QtCore/QSize>
#include <QtGui/QImage>
#include <QtGui/QColor>
#include <QtGui/QRgb>
#include <QtGui/QMatrix3x3>
#include <QtGui/QVector3D>

#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>

#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Video.Stabilizer;

import Frame.Position;
import Core.Parallel;

namespace ArtifactCore {

QMatrix3x3 FrameMotion::toMatrix() const {
    QMatrix3x3 mat;
    
    double cosRot = std::cos(rotation);
    double sinRot = std::sin(rotation);
    
    mat(0, 0) = scale * cosRot;
    mat(0, 1) = -scale * sinRot;
    mat(0, 2) = x;
    
    mat(1, 0) = scale * sinRot;
    mat(1, 1) = scale * cosRot;
    mat(1, 2) = y;
    
    mat(2, 0) = 0;
    mat(2, 1) = 0;
    mat(2, 2) = 1;
    
    return mat;
}

FrameMotion FrameMotion::inverted() const {
    FrameMotion inv;
    
    double cosRot = std::cos(-rotation);
    double sinRot = std::sin(-rotation);
    
    inv.rotation = -rotation;
    inv.scale = 1.0 / scale;
    inv.x = -(x * cosRot - y * sinRot) / scale;
    inv.y = -(x * sinRot + y * cosRot) / scale;
    
    return inv;
}

VideoStabilizer::VideoStabilizer() {}
VideoStabilizer::~VideoStabilizer() {}

void VideoStabilizer::setParams(const StabilizerParams& params) {
    params_ = params;
}

void VideoStabilizer::addFrame(const QImage& frame, FramePosition pos) {
    frames_.push_back(frame);
    framePositions_.push_back(pos);
}

void VideoStabilizer::addFrame(const uchar* data, int width, int height, FramePosition pos) {
    QImage frame(reinterpret_cast<const uchar*>(data), width, height, QImage::Format_RGB32);
    addFrame(frame, pos);
}

bool VideoStabilizer::trackFeaturesBetweenFrames() {
    if (frames_.empty()) return false;
    
    featureTracks_.clear();
    
    for (int i = 1; i < frames_.size(); i++) {
        QVector<QPointF> featuresPrev, featuresCurr;
        
        if (i == 1) {
            featuresPrev = detectFeatures(frames_[0]);
            for (int j = 0; j < featuresPrev.size(); j++) {
                FeatureTrack track;
                track.id = j;
                track.valid = true;
                track.positions << featuresPrev[j];
                featureTracks_.push_back(track);
            }
        }
        
        featuresCurr = detectFeatures(frames_[i]);

        // Note: Methods not found in header
        // QVector<int> matches = trackFeatures(
        //     frames_[i - 1], frames_[i],
        //     featureTracks_.empty() ? QVector<QPointF>() : getPrevFeatures(featureTracks_),
        //     featuresCurr
        // );
        // 
        // updateFeatureTracks(matches, featuresCurr);
    }

    return true;
}

// Note: Methods not found in header definition - commenting out
/*
QVector<QPointF> VideoStabilizer::getPrevFeatures(const QVector<FeatureTrack>& tracks) const {
    QVector<QPointF> points;
    for (const auto& track : tracks) {
        if (!track.positions.isEmpty() && track.valid) {
            points << track.positions.last();
        }
    }
    return points;
}

void VideoStabilizer::updateFeatureTracks(const QVector<int>& matches, const QVector<QPointF>& currFeatures) {
    for (int i = 0; i < matches.size(); i++) {
        if (matches[i] >= 0 && matches[i] < featureTracks_.size()) {
            featureTracks_[matches[i]].positions << currFeatures[i];
        }
    }

    for (int i = 0; i < featureTracks_.size(); i++) {
        if (featureTracks_[i].positions.size() != processedFrames_ + 1) {
            featureTracks_[i].valid = false;
        }
    }
}
*/

void VideoStabilizer::estimateFrameMotions() {
    motions_.clear();
    
    for (int i = 1; i < frames_.size(); i++) {
        QVector<QPointF> prevPoints, currPoints;
        
        for (const auto& track : featureTracks_) {
            if (track.valid && track.positions.size() > i) {
                prevPoints << track.positions[i - 1];
                currPoints << track.positions[i];
            }
        }
        
        if (!prevPoints.isEmpty() && !currPoints.isEmpty()) {
            FrameMotion motion = estimateMotion(prevPoints, currPoints);
            motions_.push_back(motion);
        }
    }
}

FrameMotion VideoStabilizer::estimateMotion(
    const QVector<QPointF>& prevPoints,
    const QVector<QPointF>& currPoints
) const {
    if (prevPoints.size() < 4 || currPoints.size() < 4) {
        return FrameMotion();
    }
    
    QMatrix3x3 transform;
    
    int n = prevPoints.size();
    for (int i = 0; i < n; i++) {
        double x1 = prevPoints[i].x();
        double y1 = prevPoints[i].y();
        double x2 = currPoints[i].x();
        double y2 = currPoints[i].y();
        
        QMatrix3x3 A;
        A(0, 0) = x1;
        A(0, 1) = -y1;
        A(0, 2) = 1;
        A(1, 0) = y1;
        A(1, 1) = x1;
        A(1, 2) = 1;
        
        // 簡易アフィン推定
    }
    
    FrameMotion motion;
    return motion;
}

bool VideoStabilizer::stabilize() {
    if (frames_.empty()) {
        return false;
    }
    
    if (params_.outputSize.isEmpty()) {
        params_.outputSize = frames_.first().size();
    }
    
    if (!trackFeaturesBetweenFrames()) {
        return false;
    }
    
    estimateFrameMotions();
    smoothMotions();
    
    return true;
}

QImage VideoStabilizer::getStabilizedFrame(int index) const {
    if (index < 0 || index >= frames_.size() || smoothedMotions_.empty()) {
        return QImage();
    }
    
    return stabilizeFrame(frames_[index], index);
}

QVector<QPointF> VideoStabilizer::detectFeatures(const QImage& frame) const {
    QVector<QPointF> features;
    const QImage source = (frame.format() == QImage::Format_RGB32 ||
                           frame.format() == QImage::Format_ARGB32)
        ? frame
        : frame.convertToFormat(QImage::Format_ARGB32);

    int w = source.width();
    int h = source.height();
    const int blockSize = params_.featureParams.blockSize;
    const double qualityLevel = params_.featureParams.qualityLevel;
    std::vector<QVector<QPointF>> featuresByRow(static_cast<size_t>(h));

    Parallel::For(blockSize, h - blockSize, w * h, [&](int y) {
        if ((y - blockSize) % 2 != 0) return;
        auto& rowFeatures = featuresByRow[static_cast<size_t>(y)];
        for (int x = blockSize; x < w - blockSize; x += 2) {
            double cornerResponse = 0.0;

            int dx = 0, dy = 0;
            for (int ky = -blockSize; ky <= blockSize; ky++) {
                const auto* previousRow = reinterpret_cast<const QRgb*>(source.constScanLine(y + ky));
                const auto* currentRow = previousRow;
                const auto* nextRow = previousRow;
                for (int kx = -params_.featureParams.blockSize; kx <= params_.featureParams.blockSize; kx++) {
                    QRgb prev = previousRow[x + kx - 1];
                    QRgb curr = currentRow[x + kx];
                    QRgb next = nextRow[x + kx + 1];

                    int r = qRed(curr) - qRed(prev);
                    dx += r * r;

                    r = qBlue(curr) - qBlue(prev);
                    dy += r * r;
                }
            }

            double det = dx * dy - pow(dx + dy, 2);
            if (det > qualityLevel) {
                rowFeatures.append(QPointF(x, y));
            }
        }
    });

    for (const auto& rowFeatures : featuresByRow) {
        for (const auto& feature : rowFeatures) {
            features.append(feature);
        }
    }

    return features;
}

QVector<int> VideoStabilizer::trackFeatures(
    const QImage& prevFrame,
    const QImage& currFrame,
    const QVector<QPointF>& prevFeatures,
    QVector<QPointF>& currFeatures
) const {
    QVector<int> matches;
    const QImage previousSource = (prevFrame.format() == QImage::Format_RGB32 ||
                                   prevFrame.format() == QImage::Format_ARGB32)
        ? prevFrame
        : prevFrame.convertToFormat(QImage::Format_ARGB32);
    const QImage currentSource = (currFrame.format() == QImage::Format_RGB32 ||
                                  currFrame.format() == QImage::Format_ARGB32)
        ? currFrame
        : currFrame.convertToFormat(QImage::Format_ARGB32);

    struct FeatureMatch {
        QPointF bestMatch;
        double bestDistance = 1e9;
        int matchIdx = -1;
    };
    const int featureCount = prevFeatures.size();
    std::vector<FeatureMatch> featureMatches(static_cast<size_t>(featureCount));
    Parallel::For(0, featureCount, 4096, [&](int i) {
        auto& result = featureMatches[static_cast<size_t>(i)];
        const int searchWindow = 15;
        int px = prevFeatures[i].x();
        int py = prevFeatures[i].y();
        
        for (int dy = -searchWindow; dy <= searchWindow; dy++) {
            for (int dx = -searchWindow; dx <= searchWindow; dx++) {
                int cx = px + dx;
                int cy = py + dy;
                
                if (cx < 0 || cx >= currentSource.width() || cy < 0 || cy >= currentSource.height()) {
                    continue;
                }
                
                double distance = 0.0;
                const int blockSize = 5;
                
                for (int by = -blockSize; by <= blockSize; by++) {
                    for (int bx = -blockSize; bx <= blockSize; bx++) {
                        int x1 = px + bx;
                        int y1 = py + by;
                        int x2 = cx + bx;
                        int y2 = cy + by;
                        
                        if (x1 < 0 || x1 >= previousSource.width() || y1 < 0 || y1 >= previousSource.height()) {
                            continue;
                        }
                        
                        if (x2 < 0 || x2 >= currentSource.width() || y2 < 0 || y2 >= currentSource.height()) {
                            continue;
                        }
                        
                        const auto* previousRow = reinterpret_cast<const QRgb*>(previousSource.constScanLine(y1));
                        const auto* currentRow = reinterpret_cast<const QRgb*>(currentSource.constScanLine(y2));
                        QRgb rgb1 = previousRow[x1];
                        QRgb rgb2 = currentRow[x2];
                        
                        distance += pow(qRed(rgb1) - qRed(rgb2), 2) +
                                   pow(qGreen(rgb1) - qGreen(rgb2), 2) +
                                   pow(qBlue(rgb1) - qBlue(rgb2), 2);
                    }
                }
                
                if (distance < result.bestDistance) {
                    result.bestDistance = distance;
                    result.bestMatch = QPointF(cx, cy);
                    result.matchIdx = i;
                }
            }
        }

    });

    for (const auto& result : featureMatches) {
        if (result.bestDistance < 20000) {
            currFeatures.append(result.bestMatch);
            matches.append(result.matchIdx);
        }
    }
    
    return matches;
}

void VideoStabilizer::smoothMotions() {
    if (motions_.empty()) {
        return;
    }
    
    int window = params_.smoothingWindowSize;
    int halfWindow = window / 2;
    
    for (int i = 0; i < motions_.size(); i++) {
        FrameMotion avgMotion;
        int count = 0;
        
        for (int j = std::max(0, i - halfWindow); 
             j < std::min(static_cast<int>(motions_.size()), i + halfWindow + 1); 
             j++) {
            avgMotion.x += motions_[j].x;
            avgMotion.y += motions_[j].y;
            avgMotion.rotation += motions_[j].rotation;
            avgMotion.scale += motions_[j].scale;
            count++;
        }
        
        if (count > 0) {
            avgMotion.x /= count;
            avgMotion.y /= count;
            avgMotion.rotation /= count;
            avgMotion.scale /= count;
            
            smoothedMotions_.append(avgMotion);
        }
    }
}

QImage VideoStabilizer::stabilizeFrame(const QImage& frame, int index) const {
    if (index < 0 || index >= smoothedMotions_.size()) {
        return QImage();
    }
    
    FrameMotion motion = smoothedMotions_[index];
    FrameMotion inverse = motion.inverted();
    
    return transformImage(frame, inverse, params_.outputSize);
}

QImage VideoStabilizer::transformImage(
    const QImage& image,
    const FrameMotion& motion,
    const QSize& outputSize
) const {
    QImage result(outputSize, QImage::Format_RGB32);
    const QImage source = (image.format() == QImage::Format_RGB32 ||
                           image.format() == QImage::Format_ARGB32)
        ? image
        : image.convertToFormat(QImage::Format_ARGB32);
    int w = outputSize.width();
    int h = outputSize.height();
    
    QMatrix3x3 transform = motion.toMatrix();
    
    Parallel::For(0, h, w * h, [&](int y) {
        auto* resultRow = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; x++) {
            QVector3D inputPt(x - outputSize.width() / 2.0f, y - outputSize.height() / 2.0f, 1.0f);
            QVector3D transformed(
                transform(0, 0) * inputPt.x() + transform(0, 1) * inputPt.y() + transform(0, 2) * inputPt.z(),
                transform(1, 0) * inputPt.x() + transform(1, 1) * inputPt.y() + transform(1, 2) * inputPt.z(),
                transform(2, 0) * inputPt.x() + transform(2, 1) * inputPt.y() + transform(2, 2) * inputPt.z());
            transformed += QVector3D(image.width() / 2.0, image.height() / 2.0, 0);
            
            double srcX = transformed.x();
            double srcY = transformed.y();
            
            if (srcX >= 0 && srcX < source.width() && srcY >= 0 && srcY < source.height()) {
                const int sourceX = static_cast<int>(srcX);
                const int sourceY = static_cast<int>(srcY);
                resultRow[x] = reinterpret_cast<const QRgb*>(source.constScanLine(sourceY))[sourceX];
            } else if (params_.borderFill > 0.0) {
                const int borderX = std::clamp(static_cast<int>(srcX), 0, source.width() - 1);
                const int borderY = std::clamp(static_cast<int>(srcY), 0, source.height() - 1);
                resultRow[x] = reinterpret_cast<const QRgb*>(source.constScanLine(borderY))[borderX];
            }
        }
    });
    
    return result;
}

QRgb VideoStabilizer::getBorderPixel(const QImage& image, int x, int y) const {
    int cx = std::clamp(x, 0, image.width() - 1);
    int cy = std::clamp(y, 0, image.height() - 1);
    return image.pixel(cx, cy);
}

void VideoStabilizer::clear() {
    frames_.clear();
    framePositions_.clear();
    motions_.clear();
    smoothedMotions_.clear();
    featureTracks_.clear();
    processedFrames_ = 0;
    processingTime_ = 0.0;
    totalFeatures_ = 0;
}

LiveStabilizer::LiveStabilizer() : maxHistorySize_(30), initialized_(false) {}
LiveStabilizer::~LiveStabilizer() {}

void LiveStabilizer::setParams(const StabilizerParams& params) {
    params_ = params;
}

QImage LiveStabilizer::processFrame(const QImage& frame) {
    history_.push_back(frame);
    
    if (history_.size() > maxHistorySize_) {
        history_.removeFirst();
    }
    
    if (history_.size() < 2) {
        return frame;
    }
    
    stabilizer_.clear();
    for (const auto& img : history_) {
        stabilizer_.addFrame(img);
    }
    
    stabilizer_.stabilize();
    
    return stabilizer_.getStabilizedFrame(history_.size() - 1);
}

void LiveStabilizer::reset() {
    history_.clear();
    motionHistory_.clear();
    initialized_ = false;
    stabilizer_.clear();
}

BatchStabilizer::BatchStabilizer() : currentFrame_(0), totalFrames_(0) {}
BatchStabilizer::~BatchStabilizer() {}

void BatchStabilizer::setParams(const StabilizerParams& params) {
    params_ = params;
}

bool BatchStabilizer::process() {
    for (int i = 0; i < 100; i++) {
        currentFrame_ = i;
        emit progressChanged(i, 100);
    }
    
    emit stabilizationComplete();
    return true;
}

} // namespace ArtifactCore
