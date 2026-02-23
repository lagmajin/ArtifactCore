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
#include <QtCore/QMatrix3x3>

export module Video.Stabilizer;

import std;
import Frame.Position;

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
        
        QVector<int> matches = trackFeatures(
            frames_[i - 1], frames_[i],
            featureTracks_.empty() ? QVector<QPointF>() : getPrevFeatures(featureTracks_),
            featuresCurr
        );
        
        updateFeatureTracks(matches, featuresCurr);
    }
    
    return true;
}

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
    
    int w = frame.width();
    int h = frame.height();
    
    for (int y = params_.featureParams.blockSize; y < h - params_.featureParams.blockSize; y += 2) {
        for (int x = params_.featureParams.blockSize; x < w - params_.featureParams.blockSize; x += 2) {
            double cornerResponse = 0.0;
            
            int dx = 0, dy = 0;
            for (int ky = -params_.featureParams.blockSize; ky <= params_.featureParams.blockSize; ky++) {
                for (int kx = -params_.featureParams.blockSize; kx <= params_.featureParams.blockSize; kx++) {
                    QRgb prev = frame.pixel(x + kx - 1, y + ky);
                    QRgb curr = frame.pixel(x + kx, y + ky);
                    QRgb next = frame.pixel(x + kx + 1, y + ky);
                    
                    int r = qRed(curr) - qRed(prev);
                    dx += r * r;
                    
                    r = qBlue(curr) - qBlue(prev);
                    dy += r * r;
                }
            }
            
            double det = dx * dy - pow(dx + dy, 2);
            if (det > params_.featureParams.qualityLevel) {
                features.append(QPointF(x, y));
            }
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
    
    for (int i = 0; i < prevFeatures.size(); i++) {
        QPointF bestMatch;
        double bestDistance = 1e9;
        int matchIdx = -1;
        
        const int searchWindow = 15;
        int px = prevFeatures[i].x();
        int py = prevFeatures[i].y();
        
        for (int dy = -searchWindow; dy <= searchWindow; dy++) {
            for (int dx = -searchWindow; dx <= searchWindow; dx++) {
                int cx = px + dx;
                int cy = py + dy;
                
                if (cx < 0 || cx >= currFrame.width() || cy < 0 || cy >= currFrame.height()) {
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
                        
                        if (x1 < 0 || x1 >= prevFrame.width() || y1 < 0 || y1 >= prevFrame.height()) {
                            continue;
                        }
                        
                        if (x2 < 0 || x2 >= currFrame.width() || y2 < 0 || y2 >= currFrame.height()) {
                            continue;
                        }
                        
                        QRgb rgb1 = prevFrame.pixel(x1, y1);
                        QRgb rgb2 = currFrame.pixel(x2, y2);
                        
                        distance += pow(qRed(rgb1) - qRed(rgb2), 2) +
                                   pow(qGreen(rgb1) - qGreen(rgb2), 2) +
                                   pow(qBlue(rgb1) - qBlue(rgb2), 2);
                    }
                }
                
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestMatch = QPointF(cx, cy);
                    matchIdx = i;
                }
            }
        }
        
        if (bestDistance < 20000) {
            currFeatures.append(bestMatch);
            matches.append(matchIdx);
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
    int w = outputSize.width();
    int h = outputSize.height();
    
    QMatrix3x3 transform = motion.toMatrix();
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            QVector3D inputPt(x - outputSize.width() / 2.0, y - outputSize.height() / 2.0, 1);
            QVector3D transformed = transform * inputPt;
            transformed += QVector3D(image.width() / 2.0, image.height() / 2.0, 0);
            
            double srcX = transformed.x();
            double srcY = transformed.y();
            
            if (srcX >= 0 && srcX < image.width() && srcY >= 0 && srcY < image.height()) {
                result.setPixel(x, y, image.pixel(srcX, srcY));
            } else if (params_.borderFill > 0.0) {
                QRgb borderPixel = getBorderPixel(image, srcX, srcY);
                result.setPixel(x, y, borderPixel);
            }
        }
    }
    
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
