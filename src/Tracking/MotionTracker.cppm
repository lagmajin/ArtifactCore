module;

#include <QString>
#include <QPointF>
#include <QRectF>
#include <QImage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QMap>
#include <vector>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <functional>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
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
#include <opencv2/opencv.hpp>
module Tracking.MotionTracker;

namespace ArtifactCore {

// ============================================================================
// TrackFrame 実装
// ============================================================================

TrackPoint* TrackFrame::findPoint(int id) {
    for (auto& p : points) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

const TrackPoint* TrackFrame::findPoint(int id) const {
    for (const auto& p : points) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

void TrackFrame::sortPointsById() {
    std::sort(points.begin(), points.end(),
              [](const TrackPoint& lhs, const TrackPoint& rhs) {
                  return lhs.id < rhs.id;
              });
}

// ============================================================================
// TrackResult 実装
// ============================================================================

void recomputeFrameConfidence(TrackFrame& frame) {
    double confidenceSum = 0.0;
    int activeCount = 0;
    for (const auto& point : frame.points) {
        if (point.active) {
            confidenceSum += std::clamp(point.confidence, 0.0, 1.0);
            ++activeCount;
        }
    }
    frame.overallConfidence = activeCount > 0
        ? confidenceSum / static_cast<double>(activeCount) : 0.0;
}

TrackFrame TrackResult::interpolateAt(double time) const {
    if (frames.empty()) return TrackFrame{};
    
    // 境界外
    if (time <= frames.front().time) return frames.front();
    if (time >= frames.back().time) return frames.back();
    
    // 補間
    for (size_t i = 0; i < frames.size() - 1; ++i) {
        if (time >= frames[i].time && time <= frames[i + 1].time) {
            double t = (time - frames[i].time) / (frames[i + 1].time - frames[i].time);
            TrackFrame result;
            result.time = time;
            result.overallConfidence = frames[i].overallConfidence * (1 - t) + frames[i + 1].overallConfidence * t;
            result.hasHomography = frames[i].hasHomography && frames[i + 1].hasHomography;
            if (result.hasHomography) {
                for (std::size_t index = 0; index < result.homography.size(); ++index) {
                    result.homography[index] =
                        frames[i].homography[index] * (1 - t) +
                        frames[i + 1].homography[index] * t;
                }
            }
            
            // ポイント補間
            for (const auto& p1 : frames[i].points) {
                const TrackPoint* p2 = frames[i + 1].findPoint(p1.id);
                if (p2) {
                    TrackPoint interp;
                    interp.id = p1.id;
                    interp.position = p1.position + (p2->position - p1.position) * t;
                    interp.velocity = p1.velocity + (p2->velocity - p1.velocity) * t;
                    interp.confidence = p1.confidence + (p2->confidence - p1.confidence) * t;
                    interp.active = p1.active && p2->active;
                    result.points.push_back(interp);
                }
            }
            result.sortPointsById();
            return result;
        }
    }
    return frames.back();
}

QPointF TrackResult::averagePosition(double time) const {
    TrackFrame frame = interpolateAt(time);
    if (frame.points.empty()) return QPointF();
    
    QPointF sum;
    int activeCount = 0;
    for (const auto& p : frame.points) {
        if (!p.active) {
            continue;
        }
        sum += p.position;
        ++activeCount;
    }
    return activeCount > 0 ? sum / activeCount : QPointF();
}

std::vector<QPointF> TrackResult::motionPath(int pointId) const {
    std::vector<QPointF> path;
    for (const auto& frame : frames) {
        const TrackPoint* p = frame.findPoint(pointId);
        if (p) {
            path.push_back(p->position);
        }
    }
    return path;
}

void TrackResult::normalize() {
    frames.erase(std::remove_if(frames.begin(), frames.end(),
                                [](const TrackFrame& frame) {
                                    return !std::isfinite(frame.time);
                                }),
                 frames.end());
    std::sort(frames.begin(), frames.end(),
              [](const TrackFrame& lhs, const TrackFrame& rhs) {
                  return lhs.time < rhs.time;
              });
    std::vector<TrackFrame> normalized;
    normalized.reserve(frames.size());
    for (auto& frame : frames) {
        frame.sortPointsById();
        if (!frame.points.empty()) {
            std::vector<TrackPoint> uniquePoints;
            uniquePoints.reserve(frame.points.size());
            for (auto& point : frame.points) {
                if (!uniquePoints.empty() && uniquePoints.back().id == point.id)
                    uniquePoints.back() = std::move(point);
                else
                    uniquePoints.push_back(std::move(point));
            }
            frame.points = std::move(uniquePoints);
        }
        bool finiteHomography = frame.hasHomography;
        if (finiteHomography) {
            for (double value : frame.homography) {
                if (!std::isfinite(value)) { finiteHomography = false; break; }
            }
        }
        frame.hasHomography = finiteHomography;
        if (!finiteHomography) frame.homography = {1.0, 0.0, 0.0,
                                                   0.0, 1.0, 0.0,
                                                   0.0, 0.0, 1.0};
        frame.overallConfidence = std::isfinite(frame.overallConfidence)
            ? std::clamp(frame.overallConfidence, 0.0, 1.0) : 0.0;
        for (auto& point : frame.points) {
            const bool finitePosition = std::isfinite(point.position.x()) &&
                                         std::isfinite(point.position.y());
            const bool finiteVelocity = std::isfinite(point.velocity.x()) &&
                                         std::isfinite(point.velocity.y());
            point.confidence = std::isfinite(point.confidence)
                ? std::clamp(point.confidence, 0.0, 1.0) : 0.0;
            if (!finitePosition || !finiteVelocity) {
                point.active = false;
                point.confidence = 0.0;
            }
        }
        if (!normalized.empty() &&
            std::abs(normalized.back().time - frame.time) < 1e-9) {
            normalized.back() = std::move(frame);
        } else {
            normalized.push_back(std::move(frame));
        }
    }
    frames = std::move(normalized);
    if (!frames.empty()) {
        startTime = frames.front().time;
        endTime = frames.back().time;
        isValid = true;
    } else {
        startTime = 0.0;
        endTime = 0.0;
        isValid = false;
    }

    auto uniqueFailures = failureFrames;
    uniqueFailures.erase(std::remove_if(uniqueFailures.begin(), uniqueFailures.end(),
                                         [](double time) { return !std::isfinite(time); }),
                         uniqueFailures.end());
    std::sort(uniqueFailures.begin(), uniqueFailures.end());
    uniqueFailures.erase(std::unique(uniqueFailures.begin(), uniqueFailures.end(),
                                     [](double lhs, double rhs) {
                                         return std::abs(lhs - rhs) < 1e-9;
                                     }),
                         uniqueFailures.end());
    failureFrames = std::move(uniqueFailures);
}

void TrackResult::setFrame(TrackFrame frame) {
    if (!std::isfinite(frame.time)) return;
    frame.sortPointsById();
    auto it = std::lower_bound(frames.begin(), frames.end(), frame.time,
                               [](const TrackFrame& lhs, double time) {
                                   return lhs.time < time;
                               });
    if (it != frames.end() && std::abs(it->time - frame.time) < 1e-9) {
        *it = std::move(frame);
    } else {
        const std::size_t index = static_cast<std::size_t>(std::distance(frames.begin(), it));
        frames.insert(frames.begin() + static_cast<std::ptrdiff_t>(index), std::move(frame));
    }
    normalize();
}

void TrackResult::addFailureFrame(double time) {
    if (!std::isfinite(time)) return;
    auto it = std::lower_bound(failureFrames.begin(), failureFrames.end(), time);
    if (it == failureFrames.end() || std::abs(*it - time) >= 1e-9) {
        failureFrames.insert(failureFrames.begin() + static_cast<std::ptrdiff_t>(std::distance(failureFrames.begin(), it)), time);
    }
}

void TrackResult::clearFailureFrames() {
    failureFrames.clear();
}

size_t TrackResult::frameCount() const {
    return frames.size();
}

// ============================================================================
// TrackerSettings 実装
// ============================================================================

TrackerSettings TrackerSettings::fast() {
    TrackerSettings s;
    s.method = TrackingMethod::FeatureBased;
    s.quality = TrackingQuality::Fast;
    s.maxFeatures = 50;
    s.maxPyramidLevel = 2;
    s.windowSize = 15;
    return s;
}

TrackerSettings TrackerSettings::normal() {
    return TrackerSettings();
}

TrackerSettings TrackerSettings::highQuality() {
    TrackerSettings s;
    s.method = TrackingMethod::OpticalFlow;
    s.quality = TrackingQuality::High;
    s.maxFeatures = 200;
    s.maxPyramidLevel = 4;
    s.windowSize = 31;
    s.subpixelIterations = 20;
    return s;
}

// ============================================================================
// TrackRegion 実装
// ============================================================================

QPointF TrackRegion::center() const {
    return bounds.center();
}

CameraSolveResult solveCameraPose(
    const std::vector<CameraCorrespondence>& correspondences,
    const CameraSolveSettings& settings) {
    CameraSolveResult result;
    if (correspondences.size() < 4) {
        result.diagnostic = QStringLiteral("Camera solve requires at least four correspondences.");
        return result;
    }
    if (!std::isfinite(settings.focalLengthX) ||
        !std::isfinite(settings.focalLengthY) ||
        settings.focalLengthX <= 0.0 || settings.focalLengthY <= 0.0 ||
        settings.iterations <= 0 || settings.reprojectionError <= 0.0 ||
        !std::isfinite(settings.confidence) || settings.confidence <= 0.0 ||
        settings.confidence > 1.0) {
        result.diagnostic = QStringLiteral("Camera intrinsics or RANSAC settings are invalid.");
        return result;
    }
    for (const double value : settings.distortion) {
        if (!std::isfinite(value)) {
            result.diagnostic = QStringLiteral("Camera distortion settings are invalid.");
            return result;
        }
    }

    std::vector<cv::Point3f> objectPoints;
    std::vector<cv::Point2f> imagePoints;
    std::vector<int> sourceIndices;
    std::vector<double> pointWeights;
    objectPoints.reserve(correspondences.size());
    imagePoints.reserve(correspondences.size());
    sourceIndices.reserve(correspondences.size());
    pointWeights.reserve(correspondences.size());
    for (std::size_t sourceIndex = 0; sourceIndex < correspondences.size(); ++sourceIndex) {
        const auto& correspondence = correspondences[sourceIndex];
        if (!std::isfinite(correspondence.weight) || correspondence.weight <= 0.0) {
            continue;
        }
        if (!std::isfinite(correspondence.imagePosition.x()) ||
            !std::isfinite(correspondence.imagePosition.y()) ||
            !std::isfinite(correspondence.worldPosition[0]) ||
            !std::isfinite(correspondence.worldPosition[1]) ||
            !std::isfinite(correspondence.worldPosition[2])) {
            continue;
        }
        objectPoints.emplace_back(
            static_cast<float>(correspondence.worldPosition[0]),
            static_cast<float>(correspondence.worldPosition[1]),
            static_cast<float>(correspondence.worldPosition[2]));
        imagePoints.emplace_back(
            static_cast<float>(correspondence.imagePosition.x()),
            static_cast<float>(correspondence.imagePosition.y()));
        sourceIndices.push_back(static_cast<int>(sourceIndex));
        pointWeights.push_back(correspondence.weight);
    }
    if (objectPoints.size() < 4) {
        result.diagnostic = QStringLiteral("Camera solve has fewer than four finite correspondences.");
        return result;
    }

    const cv::Mat cameraMatrix = (cv::Mat_<double>(3, 3) <<
        settings.focalLengthX, 0.0, settings.principalPointX,
        0.0, settings.focalLengthY, settings.principalPointY,
        0.0, 0.0, 1.0);
    cv::Mat distortion = cv::Mat::zeros(8, 1, CV_64F);
    for (std::size_t index = 0; index < settings.distortion.size(); ++index) {
        distortion.at<double>(static_cast<int>(index)) = settings.distortion[index];
    }
    cv::Mat rotationVector = cv::Mat::zeros(3, 1, CV_64F);
    cv::Mat translationVector = cv::Mat::zeros(3, 1, CV_64F);
    cv::Mat inliers;
    bool solved = false;
    try {
        solved = cv::solvePnPRansac(
            objectPoints, imagePoints, cameraMatrix, distortion,
            rotationVector, translationVector, settings.useExtrinsicGuess,
            settings.iterations, settings.reprojectionError, settings.confidence,
            inliers, cv::SOLVEPNP_ITERATIVE);
    } catch (const cv::Exception& error) {
        result.diagnostic = QStringLiteral("Camera solve failed: %1")
                                .arg(QString::fromUtf8(error.what()));
        return result;
    }
    if (!solved || rotationVector.total() != 3 || translationVector.total() != 3) {
        result.diagnostic = QStringLiteral("Camera pose could not be solved from the supplied points.");
        return result;
    }

    result.pose.rotation = {
        rotationVector.at<double>(0), rotationVector.at<double>(1),
        rotationVector.at<double>(2)};
    result.pose.translation = {
        translationVector.at<double>(0), translationVector.at<double>(1),
        translationVector.at<double>(2)};
    std::vector<int> localInliers;
    localInliers.reserve(static_cast<std::size_t>(inliers.rows));
    for (int index = 0; index < inliers.rows; ++index) {
        const int localIndex = inliers.at<int>(index);
        if (localIndex >= 0 && localIndex < static_cast<int>(sourceIndices.size())) {
            localInliers.push_back(localIndex);
            result.inlierIndices.push_back(sourceIndices[static_cast<std::size_t>(localIndex)]);
        }
    }

    std::vector<cv::Point2f> projected;
    cv::projectPoints(objectPoints, rotationVector, translationVector,
                      cameraMatrix, distortion, projected);
    double errorSum = 0.0;
    double weightSum = 0.0;
    double maxError = 0.0;
    for (const int index : localInliers) {
        if (index < 0 || index >= static_cast<int>(projected.size())) {
            continue;
        }
        const double error = cv::norm(projected[static_cast<std::size_t>(index)] -
                                      imagePoints[static_cast<std::size_t>(index)]);
        errorSum += error * pointWeights[static_cast<std::size_t>(index)];
        weightSum += pointWeights[static_cast<std::size_t>(index)];
        maxError = std::max(maxError, error);
    }
    if (result.inlierIndices.empty()) {
        result.diagnostic = QStringLiteral("Camera solve produced no inliers.");
        return result;
    }
    result.meanReprojectionError = weightSum > 0.0
        ? errorSum / weightSum : 0.0;
    result.maxReprojectionError = maxError;
    result.valid = std::isfinite(result.meanReprojectionError) &&
                   result.meanReprojectionError <= settings.reprojectionError;
    result.diagnostic = result.valid
        ? QStringLiteral("Camera pose solved with %1 inliers.").arg(result.inlierIndices.size())
        : QStringLiteral("Camera pose exceeded the reprojection error threshold.");
    return result;
}

int CameraPoseStream::validFrameCount() const {
    return static_cast<int>(std::count_if(
        frames.begin(), frames.end(), [](const CameraPoseFrame& frame) {
            if (!frame.valid || !std::isfinite(frame.time) ||
                !std::isfinite(frame.meanReprojectionError) ||
                !std::isfinite(frame.maxReprojectionError)) return false;
            for (double value : frame.pose.rotation)
                if (!std::isfinite(value)) return false;
            for (double value : frame.pose.translation)
                if (!std::isfinite(value)) return false;
            return true;
        }));
}

int CameraPoseStream::failedFrameCount() const {
    return static_cast<int>(frames.size()) - validFrameCount();
}

double CameraPoseStream::averageReprojectionError() const {
    double total = 0.0;
    int count = 0;
    for (const auto& frame : frames) {
        if (frame.valid && std::isfinite(frame.meanReprojectionError)) {
            total += frame.meanReprojectionError;
            ++count;
        }
    }
    return count > 0 ? total / static_cast<double>(count) : 0.0;
}

bool CameraPoseStream::interpolatePoseAt(double time, CameraPoseFrame& out) const {
    if (!std::isfinite(time)) return false;
    const CameraPoseFrame* previous = nullptr;
    for (const auto& frame : frames) {
        if (!frame.valid || !std::isfinite(frame.time) ||
            !std::isfinite(frame.meanReprojectionError) ||
            !std::isfinite(frame.maxReprojectionError)) continue;
        bool finitePose = true;
        for (double value : frame.pose.rotation)
            finitePose = finitePose && std::isfinite(value);
        for (double value : frame.pose.translation)
            finitePose = finitePose && std::isfinite(value);
        if (!finitePose) continue;
        if (frame.time >= time) {
            if (!previous || std::abs(frame.time - previous->time) < 1e-9) {
                out = frame;
                out.time = time;
                return true;
            }
            const double alpha = std::clamp((time - previous->time) /
                                                (frame.time - previous->time), 0.0, 1.0);
            out = *previous;
            out.time = time;
            auto lerp = [alpha](double a, double b) { return a + (b - a) * alpha; };
            for (size_t i = 0; i < out.pose.rotation.size(); ++i)
                out.pose.rotation[i] = lerp(previous->pose.rotation[i], frame.pose.rotation[i]);
            for (size_t i = 0; i < out.pose.translation.size(); ++i)
                out.pose.translation[i] = lerp(previous->pose.translation[i], frame.pose.translation[i]);
            out.meanReprojectionError = lerp(previous->meanReprojectionError, frame.meanReprojectionError);
            out.maxReprojectionError = lerp(previous->maxReprojectionError, frame.maxReprojectionError);
            out.diagnostic = QStringLiteral("Interpolated between valid camera poses.");
            return true;
        }
        previous = &frame;
    }
    if (previous) {
        out = *previous;
        out.time = time;
        return true;
    }
    return false;
}

void CameraPoseStream::normalize() {
    if (!std::isfinite(solveSettings.focalLengthX) || solveSettings.focalLengthX <= 0.0)
        solveSettings.focalLengthX = 1.0;
    if (!std::isfinite(solveSettings.focalLengthY) || solveSettings.focalLengthY <= 0.0)
        solveSettings.focalLengthY = 1.0;
    if (!std::isfinite(solveSettings.principalPointX)) solveSettings.principalPointX = 0.0;
    if (!std::isfinite(solveSettings.principalPointY)) solveSettings.principalPointY = 0.0;
    if (!std::isfinite(solveSettings.reprojectionError) || solveSettings.reprojectionError <= 0.0)
        solveSettings.reprojectionError = 8.0;
    solveSettings.confidence = std::isfinite(solveSettings.confidence)
        ? std::clamp(solveSettings.confidence, 0.0, 1.0) : 0.99;
    solveSettings.iterations = std::clamp(solveSettings.iterations, 1, 100000);
    for (double& value : solveSettings.distortion)
        if (!std::isfinite(value)) value = 0.0;
    frames.erase(std::remove_if(frames.begin(), frames.end(), [](const CameraPoseFrame& frame) {
        return !std::isfinite(frame.time);
    }), frames.end());
    std::sort(frames.begin(), frames.end(),
              [](const CameraPoseFrame& lhs, const CameraPoseFrame& rhs) {
                  return lhs.time < rhs.time;
              });
    std::vector<CameraPoseFrame> normalized;
    normalized.reserve(frames.size());
    for (auto& frame : frames) {
        if (!std::isfinite(frame.meanReprojectionError) || frame.meanReprojectionError < 0.0)
            frame.meanReprojectionError = 0.0;
        if (!std::isfinite(frame.maxReprojectionError) || frame.maxReprojectionError < 0.0)
            frame.maxReprojectionError = 0.0;
        bool finitePose = true;
        for (double& value : frame.pose.rotation) {
            if (!std::isfinite(value)) { value = 0.0; finitePose = false; }
        }
        for (double& value : frame.pose.translation) {
            if (!std::isfinite(value)) { value = 0.0; finitePose = false; }
        }
        if (!finitePose) frame.valid = false;
        if (!normalized.empty() &&
            std::abs(normalized.back().time - frame.time) < 1e-9) {
            normalized.back() = std::move(frame);
        } else {
            normalized.push_back(std::move(frame));
        }
    }
    frames = std::move(normalized);
}

CameraPoseStream solveCameraPoseStream(
    const std::vector<std::pair<double, std::vector<CameraCorrespondence>>>& samples,
    const CameraSolveSettings& settings) {
    CameraPoseStream stream;
    stream.solveSettings = settings;
    stream.frames.reserve(samples.size());
    for (const auto& sample : samples) {
        const CameraSolveResult solved = solveCameraPose(sample.second, settings);
        CameraPoseFrame frame;
        frame.time = sample.first;
        frame.pose = solved.pose;
        frame.meanReprojectionError = solved.meanReprojectionError;
        frame.maxReprojectionError = solved.maxReprojectionError;
        frame.valid = solved.valid;
        frame.diagnostic = solved.diagnostic;
        stream.frames.push_back(std::move(frame));
    }
    stream.normalize();
    return stream;
}

QString cameraPoseStreamToJson(const CameraPoseStream& stream) {
    CameraPoseStream normalized = stream;
    normalized.normalize();
    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = normalized.schemaVersion;
    root[QStringLiteral("cancelled")] = normalized.cancelled;
    QJsonObject solveSettings;
    solveSettings[QStringLiteral("focalLengthX")] = normalized.solveSettings.focalLengthX;
    solveSettings[QStringLiteral("focalLengthY")] = normalized.solveSettings.focalLengthY;
    solveSettings[QStringLiteral("principalPointX")] = normalized.solveSettings.principalPointX;
    solveSettings[QStringLiteral("principalPointY")] = normalized.solveSettings.principalPointY;
    solveSettings[QStringLiteral("reprojectionError")] = normalized.solveSettings.reprojectionError;
    solveSettings[QStringLiteral("confidence")] = normalized.solveSettings.confidence;
    solveSettings[QStringLiteral("iterations")] = normalized.solveSettings.iterations;
    solveSettings[QStringLiteral("useExtrinsicGuess")] = normalized.solveSettings.useExtrinsicGuess;
    QJsonArray distortion;
    for (const double value : normalized.solveSettings.distortion) {
        distortion.append(value);
    }
    solveSettings[QStringLiteral("distortion")] = distortion;
    root[QStringLiteral("solveSettings")] = solveSettings;
    QJsonArray frames;
    for (const auto& frame : normalized.frames) {
        QJsonObject encoded;
        encoded[QStringLiteral("time")] = frame.time;
        encoded[QStringLiteral("valid")] = frame.valid;
        encoded[QStringLiteral("meanReprojectionError")] = frame.meanReprojectionError;
        encoded[QStringLiteral("maxReprojectionError")] = frame.maxReprojectionError;
        encoded[QStringLiteral("diagnostic")] = frame.diagnostic;
        QJsonArray rotation;
        QJsonArray translation;
        for (const double value : frame.pose.rotation) {
            rotation.append(value);
        }
        for (const double value : frame.pose.translation) {
            translation.append(value);
        }
        encoded[QStringLiteral("rotation")] = rotation;
        encoded[QStringLiteral("translation")] = translation;
        frames.append(encoded);
    }
    root[QStringLiteral("frames")] = frames;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool cameraPoseStreamFromJson(const QString& json, CameraPoseStream& stream) {
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject()) {
        return false;
    }
    const QJsonObject root = document.object();
    const QJsonValue framesValue = root.value(QStringLiteral("frames"));
    if (!framesValue.isArray()) {
        return false;
    }

    CameraPoseStream decoded;
    decoded.schemaVersion = std::max(1, root.value(QStringLiteral("schemaVersion")).toInt(1));
    decoded.cancelled = root.value(QStringLiteral("cancelled")).toBool(false);
    if (root.value(QStringLiteral("solveSettings")).isObject()) {
        const QJsonObject settings = root.value(QStringLiteral("solveSettings")).toObject();
        decoded.solveSettings.focalLengthX = settings.value(QStringLiteral("focalLengthX")).toDouble(decoded.solveSettings.focalLengthX);
        decoded.solveSettings.focalLengthY = settings.value(QStringLiteral("focalLengthY")).toDouble(decoded.solveSettings.focalLengthY);
        decoded.solveSettings.principalPointX = settings.value(QStringLiteral("principalPointX")).toDouble(decoded.solveSettings.principalPointX);
        decoded.solveSettings.principalPointY = settings.value(QStringLiteral("principalPointY")).toDouble(decoded.solveSettings.principalPointY);
        decoded.solveSettings.reprojectionError = settings.value(QStringLiteral("reprojectionError")).toDouble(decoded.solveSettings.reprojectionError);
        decoded.solveSettings.confidence = settings.value(QStringLiteral("confidence")).toDouble(decoded.solveSettings.confidence);
        decoded.solveSettings.iterations = settings.value(QStringLiteral("iterations")).toInt(decoded.solveSettings.iterations);
        decoded.solveSettings.useExtrinsicGuess = settings.value(QStringLiteral("useExtrinsicGuess")).toBool(decoded.solveSettings.useExtrinsicGuess);
        const QJsonArray distortion = settings.value(QStringLiteral("distortion")).toArray();
        if (distortion.size() == static_cast<int>(decoded.solveSettings.distortion.size())) {
            for (int index = 0; index < distortion.size(); ++index) {
                decoded.solveSettings.distortion[static_cast<std::size_t>(index)] = distortion[index].toDouble();
            }
        }
    }
    if (!std::isfinite(decoded.solveSettings.focalLengthX) ||
        !std::isfinite(decoded.solveSettings.focalLengthY) ||
        !std::isfinite(decoded.solveSettings.principalPointX) ||
        !std::isfinite(decoded.solveSettings.principalPointY) ||
        !std::isfinite(decoded.solveSettings.reprojectionError) ||
        !std::isfinite(decoded.solveSettings.confidence) ||
        decoded.solveSettings.focalLengthX <= 0.0 ||
        decoded.solveSettings.focalLengthY <= 0.0 ||
        decoded.solveSettings.reprojectionError <= 0.0 ||
        decoded.solveSettings.confidence <= 0.0 ||
        decoded.solveSettings.confidence > 1.0 ||
        decoded.solveSettings.iterations <= 0) {
        return false;
    }
    for (const double value : decoded.solveSettings.distortion) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    for (const QJsonValue& value : framesValue.toArray()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject encoded = value.toObject();
        const QJsonArray rotation = encoded.value(QStringLiteral("rotation")).toArray();
        const QJsonArray translation = encoded.value(QStringLiteral("translation")).toArray();
        if (rotation.size() != 3 || translation.size() != 3) {
            return false;
        }
        CameraPoseFrame frame;
        frame.time = encoded.value(QStringLiteral("time")).toDouble();
        frame.valid = encoded.value(QStringLiteral("valid")).toBool(false);
        frame.meanReprojectionError = encoded.value(QStringLiteral("meanReprojectionError")).toDouble();
        frame.maxReprojectionError = encoded.value(QStringLiteral("maxReprojectionError")).toDouble();
        frame.diagnostic = encoded.value(QStringLiteral("diagnostic")).toString();
        for (int index = 0; index < 3; ++index) {
            frame.pose.rotation[static_cast<std::size_t>(index)] = rotation[index].toDouble();
            frame.pose.translation[static_cast<std::size_t>(index)] = translation[index].toDouble();
        }
        if (!std::isfinite(frame.time) ||
            !std::isfinite(frame.meanReprojectionError) ||
            !std::isfinite(frame.maxReprojectionError) ||
            frame.meanReprojectionError < 0.0 || frame.maxReprojectionError < 0.0) {
            return false;
        }
        for (const double value : frame.pose.rotation) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
        for (const double value : frame.pose.translation) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
        decoded.frames.push_back(std::move(frame));
    }
    decoded.normalize();
    stream = std::move(decoded);
    return true;
}

CameraSolveJob::~CameraSolveJob() {
    cancel();
    wait();
}

bool CameraSolveJob::start(Samples samples, CameraSolveSettings settings,
                           ProgressCallback progress) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_ || worker_.joinable()) {
        return false;
    }
    cancelRequested_.store(false);
    result_ = CameraPoseStream{};
    running_ = true;
    worker_ = std::thread([this, samples = std::move(samples), settings,
                           progress = std::move(progress)]() mutable {
        CameraPoseStream stream;
        stream.solveSettings = settings;
        stream.frames.reserve(samples.size());
        for (std::size_t index = 0; index < samples.size(); ++index) {
            if (cancelRequested_.load()) {
                break;
            }
            const auto solved = solveCameraPose(samples[index].second, settings);
            CameraPoseFrame frame;
            frame.time = samples[index].first;
            frame.pose = solved.pose;
            frame.meanReprojectionError = solved.meanReprojectionError;
            frame.maxReprojectionError = solved.maxReprojectionError;
            frame.valid = solved.valid;
            frame.diagnostic = solved.diagnostic;
            stream.frames.push_back(std::move(frame));
            stream.normalize();
            {
                std::lock_guard<std::mutex> resultLock(mutex_);
                result_ = stream;
            }
            if (progress) {
                try {
                    progress(samples.empty() ? 1.0
                                             : static_cast<double>(index + 1) /
                                                   static_cast<double>(samples.size()));
                } catch (...) {
                    cancelRequested_.store(true);
                }
            }
        }
        stream.normalize();
        stream.cancelled = cancelRequested_.load();
        {
            std::lock_guard<std::mutex> resultLock(mutex_);
            result_ = std::move(stream);
            running_ = false;
        }
    });
    return true;
}

void CameraSolveJob::cancel() {
    cancelRequested_.store(true);
}

void CameraSolveJob::wait() {
    std::thread completed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!worker_.joinable()) {
            return;
        }
        completed = std::move(worker_);
    }
    if (completed.joinable()) {
        completed.join();
    }
}

bool CameraSolveJob::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

CameraPoseStream CameraSolveJob::result() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return result_;
}

// ============================================================================
// MotionTracker::Impl
// ============================================================================

struct FlowMeasurement {
    QPointF displacement;
    double confidence = 0.0;
    bool valid = false;
};

class MotionTracker::Impl {
public:
    int id = 0;
    QString name;
    TrackerSettings settings;
    TrackerType type = TrackerType::Point;
    
    // 現在のトラッキングポイント
    std::vector<TrackPoint> currentPoints;
    std::vector<TrackRegion> regions;
    
    // フレームバッファ
    QMap<double, cv::Mat> frameBuffer;
    double currentTime = 0.0;
    
    // トラッキング結果
    TrackResult result;
    
    // 状態
    bool isTracking = false;
    bool shouldStop = false;
    
    int nextPointId = 1;
    int nextRegionId = 1;
    
    // QImage -> cv::Mat 変換
    cv::Mat qimageToMat(const QImage& img) {
        QImage swapped = img.convertToFormat(QImage::Format_Grayscale8);
        return cv::Mat(swapped.height(), swapped.width(), CV_8UC1,
                       const_cast<uchar*>(swapped.bits()),
                       swapped.bytesPerLine()).clone();
    }

    cv::Mat normalizeFrame(const cv::Mat& frame) {
        if (frame.empty()) return {};
        cv::Mat normalized;
        if (frame.channels() == 1) normalized = frame.clone();
        else if (frame.channels() == 3) cv::cvtColor(frame, normalized, cv::COLOR_BGR2GRAY);
        else if (frame.channels() == 4) cv::cvtColor(frame, normalized, cv::COLOR_BGRA2GRAY);
        else return {};
        if (normalized.depth() != CV_8U) {
            cv::Mat converted;
            normalized.convertTo(converted, CV_8U);
            normalized = std::move(converted);
        }
        return normalized;
    }

    QPointF applyHomography(const std::array<double, 9>& homography,
                            const QPointF& point) const {
        const double denominator = homography[6] * point.x() +
                                   homography[7] * point.y() + homography[8];
        if (std::abs(denominator) < 1e-9) {
            return point;
        }
        return QPointF(
            (homography[0] * point.x() + homography[1] * point.y() + homography[2]) /
                denominator,
            (homography[3] * point.x() + homography[4] * point.y() + homography[5]) /
                denominator);
    }

    // ECCを用いたプラナートラッキング (Homography)
    bool computePlanarHomography(const cv::Mat& prevImg, const cv::Mat& currImg, const QRectF& region, std::array<double, 9>& homographyOut, double& confidenceOut) {
        confidenceOut = 0.0;
        cv::Mat prev = normalizeFrame(prevImg);
        cv::Mat curr = normalizeFrame(currImg);
        
        cv::Mat mask = cv::Mat::zeros(prev.size(), CV_8UC1);
        cv::Rect roi(std::max(0, static_cast<int>(region.x())),
                     std::max(0, static_cast<int>(region.y())),
                     std::min(prev.cols - static_cast<int>(region.x()), static_cast<int>(region.width())),
                     std::min(prev.rows - static_cast<int>(region.y()), static_cast<int>(region.height())));
        
        if(roi.width <= 0 || roi.height <= 0) return false;
        mask(roi) = 255;
        
        cv::Mat warpMatrix = cv::Mat::eye(3, 3, CV_32F);
        
        int number_of_iterations = 50;
        double termination_eps = 1e-4;
        cv::TermCriteria criteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, number_of_iterations, termination_eps);
        
        try {
            double ecc = cv::findTransformECC(prev, curr, warpMatrix, cv::MOTION_HOMOGRAPHY, criteria, mask);
            if (ecc < 0) return false;
            confidenceOut = std::clamp(ecc, 0.0, 1.0);
            
            for (int i=0; i<3; ++i) {
                for (int j=0; j<3; ++j) {
                    homographyOut[i*3 + j] = warpMatrix.at<float>(i, j);
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    // 設定に応じた LK / NCC ベースのポイント測定
    FlowMeasurement computeOpticalFlow(const cv::Mat& prev, const cv::Mat& curr, const QPointF& point) {
        const int templateSize = 21;
        const int searchRadius = 31;
        const int halfTmpl = templateSize / 2;
        const int halfSearch = searchRadius;

        if (!settings.searchRegion.isEmpty() &&
            !settings.searchRegion.contains(point)) {
            return {};
        }

        cv::Mat prevGray = normalizeFrame(prev);
        cv::Mat currGray = normalizeFrame(curr);

        if (settings.method == TrackingMethod::OpticalFlow ||
            settings.method == TrackingMethod::FeatureBased ||
            settings.method == TrackingMethod::Hybrid) {
            const bool allowTemplateFallback = settings.method == TrackingMethod::Hybrid;
            std::vector<cv::Point2f> previousPoints{
                cv::Point2f(static_cast<float>(point.x()), static_cast<float>(point.y()))};
            std::vector<cv::Point2f> nextPoints;
            std::vector<unsigned char> status;
            std::vector<float> errors;
            try {
                cv::calcOpticalFlowPyrLK(
                    prevGray, currGray, previousPoints, nextPoints, status, errors,
                    cv::Size(settings.windowSize, settings.windowSize),
                    settings.maxPyramidLevel,
                    cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                                     settings.subpixelIterations, 0.01));
            } catch (...) {
                if (!allowTemplateFallback) return {};
            }
            if (!nextPoints.empty() && !status.empty() && status.front() &&
                std::isfinite(nextPoints.front().x) &&
                std::isfinite(nextPoints.front().y)) {
                const double trackingError = errors.empty() ? 0.0 : errors.front();
                const double confidence = 1.0 /
                    (1.0 + std::max(0.0, trackingError));
                if (trackingError <= settings.errorThreshold &&
                    confidence >= settings.confidenceThreshold) {
                    return {QPointF(nextPoints.front().x - static_cast<float>(point.x()),
                                    nextPoints.front().y - static_cast<float>(point.y())),
                            confidence, true};
                }
                if (!allowTemplateFallback) {
                    return {QPointF(), confidence, false};
                }
            } else if (!allowTemplateFallback) {
                return {};
            }
            // Hybrid mode intentionally falls through to NCC here.
        }

        int cx = static_cast<int>(std::round(point.x()));
        int cy = static_cast<int>(std::round(point.y()));

        int tx = cx - halfTmpl;
        int ty = cy - halfTmpl;
        if (tx < 0 || ty < 0 || tx + templateSize > prevGray.cols || ty + templateSize > prevGray.rows) {
            return {};
        }
        cv::Mat tmpl = prevGray(cv::Rect(tx, ty, templateSize, templateSize)).clone();

        int sx = cx - halfSearch;
        int sy = cy - halfSearch;
        sx = std::max(0, sx);
        sy = std::max(0, sy);
        int sw = std::min(halfSearch * 2, currGray.cols - sx);
        int sh = std::min(halfSearch * 2, currGray.rows - sy);
        if (sw < templateSize || sh < templateSize) {
            return {};
        }

        cv::Mat searchRegion = currGray(cv::Rect(sx, sy, sw, sh));
        cv::Mat resultMap;
        cv::matchTemplate(searchRegion, tmpl, resultMap, cv::TM_CCORR_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(resultMap, &minVal, &maxVal, &minLoc, &maxLoc);

        float peakX = static_cast<float>(sx + maxLoc.x + halfTmpl);
        float peakY = static_cast<float>(sy + maxLoc.y + halfTmpl);

        // sub-pixel (parabola fit)
        if (maxLoc.x > 0 && maxLoc.x < resultMap.cols - 1 &&
            maxLoc.y > 0 && maxLoc.y < resultMap.rows - 1) {
            auto v = [&](int dx, int dy) -> float {
                return resultMap.at<float>(maxLoc.y + dy, maxLoc.x + dx);
            };
            float c0 = v(0, 0);
            float cx1 = v(-1, 0), cx2 = v(1, 0);
            float cy1 = v(0, -1), cy2 = v(0, 1);
            float dx = (cx2 - cx1) / (2.0f * (2.0f * c0 - cx1 - cx2 + 1e-6f));
            float dy = (cy2 - cy1) / (2.0f * (2.0f * c0 - cy1 - cy2 + 1e-6f));
            if (std::isfinite(dx) && std::isfinite(dy)) {
                peakX += dx;
                peakY += dy;
            }
        }

        float conf = static_cast<float>(maxVal);
        if (conf < settings.confidenceThreshold) {
            return {QPointF(), conf, false};
        }

        return {QPointF(peakX - static_cast<float>(point.x()),
                        peakY - static_cast<float>(point.y())),
                conf, true};
    }
    
    // 特徴点検出 (Shi-Tomasi / goodFeaturesToTrack)
    std::vector<QPointF> detectFeatures(const cv::Mat& frame, int maxFeatures, double minDist) {
        std::vector<QPointF> features;
        cv::Mat gray = normalizeFrame(frame);
        std::vector<cv::Point2f> corners;
        cv::goodFeaturesToTrack(gray, corners, maxFeatures, 0.01, minDist, cv::Mat(), 3, false, 0.04);
        features.reserve(corners.size());
        for (const auto& pt : corners) {
            if (pt.x >= 0 && pt.y >= 0) {
                const QPointF candidate(pt.x, pt.y);
                if (settings.searchRegion.isEmpty() ||
                    settings.searchRegion.contains(candidate)) {
                    features.push_back(candidate);
                }
            }
        }
        return features;
    }
};

// ============================================================================
// MotionTracker 実装
// ============================================================================

MotionTracker::MotionTracker() : impl_(new Impl()) {
    static int nextId = 1;
    impl_->id = nextId++;
}

MotionTracker::~MotionTracker() {
    delete impl_;
}

MotionTracker::MotionTracker(const MotionTracker& other) 
    : impl_(new Impl(*other.impl_)) {}

MotionTracker& MotionTracker::operator=(const MotionTracker& other) {
    if (this != &other) {
        delete impl_;
        impl_ = new Impl(*other.impl_);
    }
    return *this;
}

MotionTracker::MotionTracker(MotionTracker&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

MotionTracker& MotionTracker::operator=(MotionTracker&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

// ========================================
// 設定
// ========================================

void MotionTracker::setSettings(const TrackerSettings& settings) {
    impl_->settings = settings;
    impl_->settings.method = static_cast<TrackingMethod>(
        std::clamp(static_cast<int>(impl_->settings.method), 0, 3));
    impl_->settings.quality = static_cast<TrackingQuality>(
        std::clamp(static_cast<int>(impl_->settings.quality), 0, 3));
    impl_->settings.type = static_cast<TrackerType>(
        std::clamp(static_cast<int>(impl_->settings.type), 0, 3));
    impl_->settings.maxFeatures = std::max(1, impl_->settings.maxFeatures);
    impl_->settings.minDistance = std::max(1.0, impl_->settings.minDistance);
    impl_->settings.windowSize = std::max(3, impl_->settings.windowSize | 1);
    impl_->settings.maxPyramidLevel = std::clamp(impl_->settings.maxPyramidLevel, 0, 8);
    impl_->settings.confidenceThreshold =
        std::clamp(impl_->settings.confidenceThreshold, 0.0, 1.0);
    impl_->settings.errorThreshold = std::max(0.0, impl_->settings.errorThreshold);
    impl_->settings.subpixelIterations =
        std::max(1, impl_->settings.subpixelIterations);
}

TrackerSettings MotionTracker::settings() const {
    return impl_->settings;
}

void MotionTracker::setTrackerType(TrackerType type) {
    impl_->type = static_cast<TrackerType>(
        std::clamp(static_cast<int>(type), 0, 3));
}

TrackerType MotionTracker::trackerType() const {
    return impl_->type;
}

void MotionTracker::setName(const QString& name) {
    impl_->name = name;
}

QString MotionTracker::name() const {
    return impl_->name;
}

int MotionTracker::id() const {
    return impl_->id;
}

// ========================================
// トラッキング領域設定
// ========================================

int MotionTracker::addTrackPoint(const QPointF& point) {
    if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
        return -1;
    }
    TrackPoint tp;
    tp.id = impl_->nextPointId++;
    tp.position = point;
    tp.confidence = 1.0;
    tp.active = true;
    impl_->currentPoints.push_back(tp);
    return tp.id;
}

void MotionTracker::addTrackPoints(const std::vector<QPointF>& points) {
    for (const auto& p : points) {
        addTrackPoint(p);
    }
}

int MotionTracker::addTrackRegion(const QRectF& bounds) {
    if (!std::isfinite(bounds.x()) || !std::isfinite(bounds.y()) ||
        !std::isfinite(bounds.width()) || !std::isfinite(bounds.height()) ||
        bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return -1;
    }
    TrackRegion region;
    region.id = impl_->nextRegionId++;
    region.bounds = bounds;
    impl_->regions.push_back(region);
    return region.id;
}

void MotionTracker::removeTrackPoint(int pointId) {
    impl_->currentPoints.erase(
        std::remove_if(impl_->currentPoints.begin(), impl_->currentPoints.end(),
                       [pointId](const TrackPoint& p) { return p.id == pointId; }),
        impl_->currentPoints.end()
    );
}

void MotionTracker::removeTrackRegion(int regionId) {
    impl_->regions.erase(
        std::remove_if(impl_->regions.begin(), impl_->regions.end(),
                       [regionId](const TrackRegion& r) { return r.id == regionId; }),
        impl_->regions.end()
    );
}

void MotionTracker::clearTrackPoints() {
    impl_->currentPoints.clear();
}

void MotionTracker::clearTrackRegions() {
    impl_->regions.clear();
}

int MotionTracker::trackPointCount() const {
    return static_cast<int>(impl_->currentPoints.size());
}

int MotionTracker::trackRegionCount() const {
    return static_cast<int>(impl_->regions.size());
}

TrackPoint MotionTracker::trackPoint(int index) const {
    if (index >= 0 && index < static_cast<int>(impl_->currentPoints.size())) {
        return impl_->currentPoints[index];
    }
    return TrackPoint();
}

std::vector<TrackPoint> MotionTracker::trackPoints() const {
    return impl_->currentPoints;
}

// ========================================
// トラッキング実行
// ========================================

void MotionTracker::setFrame(double time, const QImage& frame) {
    setFrame(time, impl_->qimageToMat(frame));
}

void MotionTracker::setFrame(double time, const cv::Mat& frame) {
    if (!std::isfinite(time)) {
        return;
    }
    cv::Mat normalized = impl_->normalizeFrame(frame);
    if (normalized.empty()) {
        impl_->frameBuffer.remove(time);
        return;
    }
    impl_->frameBuffer[time] = std::move(normalized);
    impl_->currentTime = time;
}

bool MotionTracker::trackForward(double fromTime, double toTime) {
    if (!std::isfinite(fromTime) || !std::isfinite(toTime) ||
        std::abs(toTime - fromTime) < 1e-9) {
        return false;
    }
    auto it1 = impl_->frameBuffer.find(fromTime);
    auto it2 = impl_->frameBuffer.find(toTime);
    
    if (it1 == impl_->frameBuffer.end() || it2 == impl_->frameBuffer.end()) {
        return false;
    }
    
    // トラッカー種別に応じた処理
    if (impl_->type == TrackerType::Planar && !impl_->regions.empty()) {
        std::array<double, 9> h = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
        double planarConfidence = 0.0;
        if (impl_->computePlanarHomography(it1.value(), it2.value(), impl_->regions.front().bounds, h, planarConfidence)) {
            for (auto& point : impl_->currentPoints) {
                const QPointF previousPosition = point.position;
                point.position = impl_->applyHomography(h, previousPosition);
                const double signedDelta = std::abs(toTime - fromTime) > 1e-9
                    ? (toTime - fromTime) : 1e-9;
                point.velocity = (point.position - previousPosition) /
                                 signedDelta;
                point.confidence = planarConfidence;
                point.active = planarConfidence >= impl_->settings.confidenceThreshold;
            }
            TrackFrame frame;
            frame.time = toTime;
            frame.homography = h;
            frame.hasHomography = true;
            frame.points = impl_->currentPoints;
            frame.overallConfidence = planarConfidence;
            if (planarConfidence < impl_->settings.confidenceThreshold) {
                impl_->result.addFailureFrame(toTime);
            }
            impl_->result.setFrame(std::move(frame));
            return true;
        }
    }

    // オプティカルフロー計算
    double confidenceSum = 0.0;
    int measuredPoints = 0;
    const double deltaTime = std::max(std::abs(toTime - fromTime), 1e-9);
    for (auto& point : impl_->currentPoints) {
        const FlowMeasurement measurement =
            impl_->computeOpticalFlow(it1.value(), it2.value(), point.position);
        point.confidence = measurement.confidence;
        point.active = measurement.valid;
        if (measurement.valid) {
            point.position += measurement.displacement;
            point.velocity = measurement.displacement / deltaTime;
            confidenceSum += measurement.confidence;
            ++measuredPoints;
        }
    }
    
    // 結果保存
    TrackFrame frame;
    frame.time = toTime;
    frame.points = impl_->currentPoints;
    frame.overallConfidence = measuredPoints > 0
        ? confidenceSum / static_cast<double>(measuredPoints)
        : 0.0;
    if (measuredPoints == 0) {
        impl_->result.addFailureFrame(toTime);
    }
    impl_->result.setFrame(std::move(frame));
    
    return true;
}

bool MotionTracker::trackBackward(double fromTime, double toTime) {
    if (!std::isfinite(fromTime) || !std::isfinite(toTime) ||
        std::abs(toTime - fromTime) < 1e-9) {
        return false;
    }
    auto it1 = impl_->frameBuffer.find(fromTime);
    auto it2 = impl_->frameBuffer.find(toTime);
    
    if (it1 == impl_->frameBuffer.end() || it2 == impl_->frameBuffer.end()) {
        return false;
    }
    
    // トラッカー種別に応じた処理
    if (impl_->type == TrackerType::Planar && !impl_->regions.empty()) {
        std::array<double, 9> h = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
        double planarConfidence = 0.0;
        if (impl_->computePlanarHomography(it2.value(), it1.value(), impl_->regions.front().bounds, h, planarConfidence)) {
            for (auto& point : impl_->currentPoints) {
                const QPointF previousPosition = point.position;
                point.position = impl_->applyHomography(h, previousPosition);
                const double signedDelta = std::abs(toTime - fromTime) > 1e-9
                    ? (toTime - fromTime) : -1e-9;
                point.velocity = (point.position - previousPosition) /
                                 signedDelta;
                point.confidence = planarConfidence;
                point.active = planarConfidence >= impl_->settings.confidenceThreshold;
            }
            TrackFrame frame;
            frame.time = toTime;
            frame.homography = h;
            frame.hasHomography = true;
            frame.points = impl_->currentPoints;
            frame.overallConfidence = planarConfidence;
            if (planarConfidence < impl_->settings.confidenceThreshold) {
                impl_->result.addFailureFrame(toTime);
            }
            impl_->result.setFrame(std::move(frame));
            return true;
        }
    }

    // 逆方向トラッキング: 入力フレーム自体を逆順にして測定する。
    double confidenceSum = 0.0;
    int measuredPoints = 0;
    const double deltaTime = std::max(std::abs(toTime - fromTime), 1e-9);
    for (auto& point : impl_->currentPoints) {
        const FlowMeasurement measurement =
            impl_->computeOpticalFlow(it2.value(), it1.value(), point.position);
        point.confidence = measurement.confidence;
        point.active = measurement.valid;
        if (measurement.valid) {
            point.position += measurement.displacement;
            point.velocity = measurement.displacement / deltaTime;
            confidenceSum += measurement.confidence;
            ++measuredPoints;
        }
    }
    
    TrackFrame frame;
    frame.time = toTime;
    frame.points = impl_->currentPoints;
    frame.overallConfidence = measuredPoints > 0
        ? confidenceSum / static_cast<double>(measuredPoints)
        : 0.0;
    if (measuredPoints == 0) {
        impl_->result.addFailureFrame(toTime);
    }
    impl_->result.setFrame(std::move(frame));
    
    return true;
}

bool MotionTracker::trackRange(double startTime, double endTime, 
                               std::function<bool(double progress)> progressCallback) {
    if (!std::isfinite(startTime) || !std::isfinite(endTime) ||
        endTime < startTime) {
        return false;
    }
    impl_->isTracking = true;
    impl_->shouldStop = false;
    impl_->result = TrackResult();
    impl_->result.trackerId = impl_->id;
    impl_->result.name = impl_->name;
    
    // 時間順にソート
    QList<double> times = impl_->frameBuffer.keys();
    std::sort(times.begin(), times.end());
    
    int totalSteps = 0;
    int currentStep = 0;
    
    for (double t : times) {
        if (t >= startTime && t <= endTime) totalSteps++;
    }
    if (totalSteps == 0) {
        impl_->isTracking = false;
        return false;
    }

    if (impl_->currentPoints.empty() &&
        impl_->settings.method == TrackingMethod::FeatureBased) {
        for (double time : times) {
            if (time < startTime || time > endTime) {
                continue;
            }
            const auto detected = impl_->detectFeatures(
                impl_->frameBuffer.value(time), impl_->settings.maxFeatures,
                impl_->settings.minDistance);
            impl_->currentPoints.clear();
            impl_->nextPointId = 1;
            for (const auto& point : detected) {
                addTrackPoint(point);
            }
            if (impl_->currentPoints.empty()) {
                impl_->isTracking = false;
                return false;
            }
            break;
        }
    }

    bool seededStartFrame = false;
    double previousTrackedTime = 0.0;
    for (double t : times) {
        if (impl_->shouldStop) break;
        if (t < startTime || t > endTime) continue;
        
        const bool isFirstSample = !seededStartFrame;
        if (isFirstSample) {
            TrackFrame startFrame;
            startFrame.time = t;
            startFrame.points = impl_->currentPoints;
            startFrame.overallConfidence = 1.0;
            impl_->result.setFrame(std::move(startFrame));
            seededStartFrame = true;
            previousTrackedTime = t;
        }
        if (!isFirstSample) {
            trackForward(previousTrackedTime, t);
            previousTrackedTime = t;
        }
        
        currentStep++;
        if (progressCallback) {
            try {
                if (!progressCallback(static_cast<double>(currentStep) / totalSteps)) {
                    impl_->shouldStop = true;
                    break;
                }
            } catch (...) {
                impl_->shouldStop = true;
                break;
            }
        }
    }
    
    impl_->result.startTime = startTime;
    impl_->result.endTime = endTime;
    impl_->result.normalize();
    impl_->isTracking = false;
    
    // A user cancellation or a progress callback abort is not a successful solve,
    // even when the partial result already contains valid frames.
    return !impl_->shouldStop && impl_->result.isValid;
}

bool MotionTracker::trackAll(std::function<bool(double progress)> progressCallback) {
    if (impl_->frameBuffer.isEmpty()) return false;
    
    QList<double> times = impl_->frameBuffer.keys();
    return trackRange(times.first(), times.last(), progressCallback);
}

void MotionTracker::stopTracking() {
    impl_->shouldStop = true;
}

bool MotionTracker::isTracking() const {
    return impl_->isTracking;
}

void MotionTracker::resetTracking() {
    impl_->result = TrackResult();
    impl_->result.trackerId = impl_->id;
    impl_->result.name = impl_->name;
}

void MotionTracker::clearTrackingData() {
    impl_->frameBuffer.clear();
    impl_->currentPoints.clear();
    impl_->regions.clear();
    resetTracking();
}

// ========================================
// 結果取得
// ========================================

TrackResult MotionTracker::result() const {
    return impl_->result;
}

QPointF MotionTracker::pointPositionAt(int pointId, double time) const {
    TrackFrame frame = impl_->result.interpolateAt(time);
    const TrackPoint* p = frame.findPoint(pointId);
    return p ? p->position : QPointF();
}

std::vector<QPointF> MotionTracker::allPointPositionsAt(double time) const {
    TrackFrame frame = impl_->result.interpolateAt(time);
    std::vector<QPointF> positions;
    positions.reserve(frame.points.size());
    for (const auto& p : frame.points) {
        positions.push_back(p.position);
    }
    return positions;
}

QPointF MotionTracker::displacementAt(double time) const {
    if (impl_->result.frames.empty()) return QPointF();
    
    QPointF startPos = impl_->result.averagePosition(impl_->result.startTime);
    QPointF currentPos = impl_->result.averagePosition(time);
    
    return currentPos - startPos;
}

double MotionTracker::rotationAt(double time) const {
    if (impl_->result.frames.empty()) {
        return 0.0;
    }

    const TrackFrame reference = impl_->result.interpolateAt(impl_->result.startTime);
    const TrackFrame current = impl_->result.interpolateAt(time);
    if (current.hasHomography) {
        const auto& h = current.homography;
        return std::atan2(h[3], h[0]) * 180.0 / std::numbers::pi;
    }

    double sinSum = 0.0;
    double cosSum = 0.0;
    int pairCount = 0;
    for (std::size_t i = 0; i < reference.points.size(); ++i) {
        const auto* currentA = current.findPoint(reference.points[i].id);
        if (!currentA || !reference.points[i].active || !currentA->active) {
            continue;
        }
        for (std::size_t j = i + 1; j < reference.points.size(); ++j) {
            const auto* currentB = current.findPoint(reference.points[j].id);
            if (!currentB || !reference.points[j].active || !currentB->active) {
                continue;
            }
            const QPointF sourceDelta = reference.points[j].position - reference.points[i].position;
            const QPointF trackedDelta = currentB->position - currentA->position;
            if (std::hypot(sourceDelta.x(), sourceDelta.y()) < 1e-6 ||
                std::hypot(trackedDelta.x(), trackedDelta.y()) < 1e-6) {
                continue;
            }
            const double angle = std::atan2(trackedDelta.y(), trackedDelta.x()) -
                                 std::atan2(sourceDelta.y(), sourceDelta.x());
            sinSum += std::sin(angle);
            cosSum += std::cos(angle);
            ++pairCount;
        }
    }
    return pairCount > 0 ? std::atan2(sinSum, cosSum) * 180.0 / std::numbers::pi : 0.0;
}

QPointF MotionTracker::scaleAt(double time) const {
    if (impl_->result.frames.empty()) {
        return QPointF(1.0, 1.0);
    }

    const TrackFrame reference = impl_->result.interpolateAt(impl_->result.startTime);
    const TrackFrame current = impl_->result.interpolateAt(time);
    if (current.hasHomography) {
        const auto& h = current.homography;
        return QPointF(std::hypot(h[0], h[3]), std::hypot(h[1], h[4]));
    }

    double scaleSum = 0.0;
    int pairCount = 0;
    for (std::size_t i = 0; i < reference.points.size(); ++i) {
        const auto* currentA = current.findPoint(reference.points[i].id);
        if (!currentA || !reference.points[i].active || !currentA->active) {
            continue;
        }
        for (std::size_t j = i + 1; j < reference.points.size(); ++j) {
            const auto* currentB = current.findPoint(reference.points[j].id);
            if (!currentB || !reference.points[j].active || !currentB->active) {
                continue;
            }
            const double referenceDistance = std::hypot(
                reference.points[j].position.x() - reference.points[i].position.x(),
                reference.points[j].position.y() - reference.points[i].position.y());
            const double currentDistance = std::hypot(
                currentB->position.x() - currentA->position.x(),
                currentB->position.y() - currentA->position.y());
            if (referenceDistance < 1e-6 || currentDistance < 1e-6) {
                continue;
            }
            scaleSum += currentDistance / referenceDistance;
            ++pairCount;
        }
    }
    const double uniformScale = pairCount > 0 ? scaleSum / pairCount : 1.0;
    return QPointF(uniformScale, uniformScale);
}

std::vector<QPointF> MotionTracker::motionPath(int pointId) const {
    return impl_->result.motionPath(pointId);
}

std::vector<std::pair<double, QPointF>> MotionTracker::exportKeyframes(int pointId) const {
    std::vector<std::pair<double, QPointF>> keyframes;
    for (const auto& frame : impl_->result.frames) {
        const TrackPoint* p = frame.findPoint(pointId);
        if (p && p->active) {
            keyframes.push_back({frame.time, p->position});
        }
    }
    return keyframes;
}

bool MotionTracker::hasResult() const {
    return impl_->result.isValid && !impl_->result.frames.empty();
}

// ========================================
// 統計・解析
// ========================================

double MotionTracker::averageConfidence() const {
    if (impl_->result.frames.empty()) return 0.0;
    
    double sum = 0.0;
    int count = 0;
    for (const auto& frame : impl_->result.frames) {
        for (const auto& p : frame.points) {
            if (!p.active) {
                continue;
            }
            sum += p.confidence;
            ++count;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

double MotionTracker::qualityScore() const {
    return averageConfidence();
}

std::vector<double> MotionTracker::problemFrames() const {
    std::vector<double> problems;
    problems.reserve(impl_->result.failureFrames.size() + impl_->result.frames.size());
    for (const double time : impl_->result.failureFrames) {
        if (std::isfinite(time)) problems.push_back(time);
    }
    for (const auto& frame : impl_->result.frames) {
        if (!std::isfinite(frame.overallConfidence) ||
            frame.overallConfidence < impl_->settings.confidenceThreshold) {
            problems.push_back(frame.time);
        }
    }
    std::sort(problems.begin(), problems.end());
    problems.erase(std::unique(problems.begin(), problems.end(),
                               [](double lhs, double rhs) {
                                   return std::abs(lhs - rhs) < 1e-9;
                               }), problems.end());
    return problems;
}

QPointF MotionTracker::averageVelocity() const {
    if (impl_->result.frames.size() < 2) return QPointF();
    
    QPointF totalVelocity;
    int count = 0;
    for (const auto& frame : impl_->result.frames) {
        for (const auto& p : frame.points) {
            if (!p.active) {
                continue;
            }
            totalVelocity += p.velocity;
            ++count;
        }
    }
    return count > 0 ? totalVelocity / count : QPointF();
}

double MotionTracker::totalDistance(int pointId) const {
    auto path = motionPath(pointId);
    double distance = 0.0;
    for (size_t i = 1; i < path.size(); ++i) {
        QPointF diff = path[i] - path[i-1];
        distance += std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
    }
    return distance;
}

// ========================================
// 補正
// ========================================

void MotionTracker::filterByConfidence(double threshold) {
    for (auto& frame : impl_->result.frames) {
        bool frameFailed = false;
        for (auto& p : frame.points) {
            if (p.confidence < threshold) {
                p.active = false;
                frameFailed = true;
            }
        }
        if (frameFailed) {
            impl_->result.addFailureFrame(frame.time);
        }
        recomputeFrameConfidence(frame);
    }
}

void MotionTracker::smoothTrack(int windowSize) {
    if (impl_->result.frames.size() < static_cast<size_t>(windowSize)) return;
    
    // 移動平均フィルタ
    int halfWindow = windowSize / 2;
    std::vector<TrackFrame> smoothed = impl_->result.frames;
    
    for (size_t i = halfWindow; i < impl_->result.frames.size() - halfWindow; ++i) {
        for (auto& point : smoothed[i].points) {
            if (!point.active) {
                continue;
            }
            QPointF sum;
            double confidenceSum = 0.0;
            int count = 0;
            for (int j = -halfWindow; j <= halfWindow; ++j) {
                const TrackPoint* p = impl_->result.frames[i + j].findPoint(point.id);
                if (p && p->active) {
                    sum += p->position;
                    confidenceSum += p->confidence;
                    ++count;
                }
            }
            if (count > 0) {
                point.position = sum / count;
                point.confidence = confidenceSum / static_cast<double>(count);
            }
        }
    }
    
    impl_->result.frames = smoothed;
    for (auto& frame : impl_->result.frames) {
        recomputeFrameConfidence(frame);
    }
}

void MotionTracker::removeOutliers(double threshold) {
    // 統計的な外れ値除去
    for (auto& frame : impl_->result.frames) {
        bool frameFailed = false;
        for (auto& p : frame.points) {
            // 速度から外れ値判定
            double speed = std::sqrt(p.velocity.x() * p.velocity.x() + 
                                    p.velocity.y() * p.velocity.y());
            if (speed > threshold) {
                p.active = false;
                p.confidence = 0.0;
                frameFailed = true;
            }
        }
        if (frameFailed) {
            impl_->result.addFailureFrame(frame.time);
        }
        recomputeFrameConfidence(frame);
    }
}

void MotionTracker::applyCorrection(double time, int pointId, const QPointF& correctedPosition) {
    if (!std::isfinite(time) || !std::isfinite(correctedPosition.x()) ||
        !std::isfinite(correctedPosition.y())) {
        return;
    }
    for (auto& frame : impl_->result.frames) {
        if (std::abs(frame.time - time) < 0.001) {
            TrackPoint* p = frame.findPoint(pointId);
            if (p) {
                p->position = correctedPosition;
                p->confidence = 1.0;
                p->active = true;
                recomputeFrameConfidence(frame);
                if (frame.overallConfidence >= impl_->settings.confidenceThreshold) {
                    impl_->result.failureFrames.erase(
                        std::remove_if(impl_->result.failureFrames.begin(),
                                       impl_->result.failureFrames.end(),
                                       [time](double failureTime) {
                                           return std::abs(failureTime - time) < 0.001;
                                       }),
                        impl_->result.failureFrames.end());
                }
            }
        }
    }
}

// ========================================
// シリアライズ
// ========================================

bool MotionTracker::saveToFile(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    
    file.write(toJson().toUtf8());
    file.close();
    return true;
}

bool MotionTracker::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    bool success = fromJson(QString::fromUtf8(file.readAll()));
    file.close();
    return success;
}

QString MotionTracker::toJson() const {
    QJsonObject root;
    root["schemaVersion"] = impl_->result.schemaVersion;
    root["id"] = impl_->id;
    root["name"] = impl_->name;
    root["trackerType"] = static_cast<int>(impl_->type);
    root["sourceName"] = impl_->result.sourceName.isEmpty() ? impl_->name : impl_->result.sourceName;
    root["sourcePath"] = impl_->result.sourcePath;
    root["sourceType"] = impl_->result.sourceType;
    root["startTime"] = impl_->result.startTime;
    root["endTime"] = impl_->result.endTime;
    root["resultValid"] = impl_->result.isValid;
    QJsonObject settingsObj;
    settingsObj["method"] = static_cast<int>(impl_->settings.method);
    settingsObj["quality"] = static_cast<int>(impl_->settings.quality);
    settingsObj["type"] = static_cast<int>(impl_->settings.type);
    settingsObj["maxFeatures"] = impl_->settings.maxFeatures;
    settingsObj["minDistance"] = impl_->settings.minDistance;
    settingsObj["windowSize"] = impl_->settings.windowSize;
    settingsObj["maxPyramidLevel"] = impl_->settings.maxPyramidLevel;
    settingsObj["confidenceThreshold"] = impl_->settings.confidenceThreshold;
    settingsObj["errorThreshold"] = impl_->settings.errorThreshold;
    settingsObj["trackForward"] = impl_->settings.trackForward;
    settingsObj["trackBackward"] = impl_->settings.trackBackward;
    settingsObj["subpixelAccuracy"] = impl_->settings.subpixelAccuracy;
    settingsObj["subpixelIterations"] = impl_->settings.subpixelIterations;
    QJsonObject searchRegionObj;
    searchRegionObj["x"] = impl_->settings.searchRegion.x();
    searchRegionObj["y"] = impl_->settings.searchRegion.y();
    searchRegionObj["width"] = impl_->settings.searchRegion.width();
    searchRegionObj["height"] = impl_->settings.searchRegion.height();
    settingsObj["searchRegion"] = searchRegionObj;
    root["settings"] = settingsObj;

    QJsonArray pointsArray;
    for (const auto& point : impl_->currentPoints) {
        QJsonObject pointObj;
        pointObj["id"] = point.id;
        pointObj["x"] = point.position.x();
        pointObj["y"] = point.position.y();
        pointObj["vx"] = point.velocity.x();
        pointObj["vy"] = point.velocity.y();
        pointObj["confidence"] = point.confidence;
        pointObj["active"] = point.active;
        pointsArray.append(pointObj);
    }
    root["trackPoints"] = pointsArray;

    QJsonArray regionsArray;
    for (const auto& region : impl_->regions) {
        QJsonObject regionObj;
        regionObj["id"] = region.id;
        regionObj["x"] = region.bounds.x();
        regionObj["y"] = region.bounds.y();
        regionObj["w"] = region.bounds.width();
        regionObj["h"] = region.bounds.height();
        QJsonArray regionPointsArray;
        for (const auto& p : region.points) {
            QJsonObject pointObj;
            pointObj["x"] = p.x();
            pointObj["y"] = p.y();
            regionPointsArray.append(pointObj);
        }
        regionObj["points"] = regionPointsArray;
        regionsArray.append(regionObj);
    }
    root["trackRegions"] = regionsArray;

    QJsonArray failureFramesArray;
    for (double time : impl_->result.failureFrames) {
        failureFramesArray.append(time);
    }
    root["failureFrames"] = failureFramesArray;
    
    QJsonArray framesArray;
    for (const auto& frame : impl_->result.frames) {
        QJsonObject frameObj;
        frameObj["time"] = frame.time;
        frameObj["confidence"] = frame.overallConfidence;
        frameObj["hasHomography"] = frame.hasHomography;
        if (frame.hasHomography) {
            QJsonArray homographyArray;
            for (const double value : frame.homography) {
                homographyArray.append(value);
            }
            frameObj["homography"] = homographyArray;
        }
        
        QJsonArray pointsArray;
        for (const auto& p : frame.points) {
            QJsonObject pointObj;
            pointObj["id"] = p.id;
            pointObj["x"] = p.position.x();
            pointObj["y"] = p.position.y();
            pointObj["vx"] = p.velocity.x();
            pointObj["vy"] = p.velocity.y();
            pointObj["confidence"] = p.confidence;
            pointObj["active"] = p.active;
            pointsArray.append(pointObj);
        }
        frameObj["points"] = pointsArray;
        framesArray.append(frameObj);
    }
    root["frames"] = framesArray;
    
    return QJsonDocument(root).toJson();
}

bool MotionTracker::fromJson(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull() || !doc.isObject()) return false;
    
    QJsonObject root = doc.object();
    const int schemaVersion = root["schemaVersion"].toInt(1);
    impl_->id = root["id"].toInt();
    impl_->name = root["name"].toString();
    impl_->type = static_cast<TrackerType>(root["trackerType"].toInt(static_cast<int>(TrackerType::Point)));
    impl_->result = TrackResult();
    impl_->result.schemaVersion = schemaVersion;
    impl_->result.sourceName = root["sourceName"].toString(impl_->name);
    impl_->result.sourcePath = root["sourcePath"].toString();
    impl_->result.sourceType = root["sourceType"].toString();
    impl_->result.startTime = root["startTime"].toDouble(0.0);
    impl_->result.endTime = root["endTime"].toDouble(0.0);
    impl_->result.isValid = root["resultValid"].toBool(false);
    setTrackerType(impl_->type);

    if (root.contains("settings") && root["settings"].isObject()) {
        const QJsonObject settingsObj = root["settings"].toObject();
        impl_->settings.method = static_cast<TrackingMethod>(settingsObj["method"].toInt(static_cast<int>(TrackingMethod::OpticalFlow)));
        impl_->settings.quality = static_cast<TrackingQuality>(settingsObj["quality"].toInt(static_cast<int>(TrackingQuality::Normal)));
        impl_->settings.type = static_cast<TrackerType>(settingsObj["type"].toInt(static_cast<int>(TrackerType::Point)));
        impl_->settings.maxFeatures = settingsObj["maxFeatures"].toInt(impl_->settings.maxFeatures);
        impl_->settings.minDistance = settingsObj["minDistance"].toDouble(impl_->settings.minDistance);
        impl_->settings.windowSize = settingsObj["windowSize"].toInt(impl_->settings.windowSize);
        impl_->settings.maxPyramidLevel = settingsObj["maxPyramidLevel"].toInt(impl_->settings.maxPyramidLevel);
        impl_->settings.confidenceThreshold = settingsObj["confidenceThreshold"].toDouble(impl_->settings.confidenceThreshold);
        impl_->settings.errorThreshold = settingsObj["errorThreshold"].toDouble(impl_->settings.errorThreshold);
        impl_->settings.trackForward = settingsObj["trackForward"].toBool(impl_->settings.trackForward);
        impl_->settings.trackBackward = settingsObj["trackBackward"].toBool(impl_->settings.trackBackward);
        impl_->settings.subpixelAccuracy = settingsObj["subpixelAccuracy"].toBool(impl_->settings.subpixelAccuracy);
        impl_->settings.subpixelIterations = settingsObj["subpixelIterations"].toInt(impl_->settings.subpixelIterations);
        if (settingsObj["searchRegion"].isObject()) {
            const QJsonObject searchRegionObj = settingsObj["searchRegion"].toObject();
            impl_->settings.searchRegion = QRectF(
                searchRegionObj["x"].toDouble(), searchRegionObj["y"].toDouble(),
                searchRegionObj["width"].toDouble(), searchRegionObj["height"].toDouble());
        }
    }
    setSettings(impl_->settings);

    impl_->currentPoints.clear();
    impl_->regions.clear();
    if (root.contains("trackPoints") && root["trackPoints"].isArray()) {
        for (const auto& pointVal : root["trackPoints"].toArray()) {
            const QJsonObject pointObj = pointVal.toObject();
            TrackPoint point;
            point.id = pointObj["id"].toInt();
            point.position = QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble());
            point.velocity = QPointF(pointObj["vx"].toDouble(), pointObj["vy"].toDouble());
            point.confidence = pointObj["confidence"].toDouble(1.0);
            point.active = pointObj["active"].toBool(true);
            impl_->currentPoints.push_back(point);
        }
    }
    if (root.contains("trackRegions") && root["trackRegions"].isArray()) {
        for (const auto& regionVal : root["trackRegions"].toArray()) {
            const QJsonObject regionObj = regionVal.toObject();
            TrackRegion region;
            region.id = regionObj["id"].toInt();
            region.bounds = QRectF(regionObj["x"].toDouble(),
                                   regionObj["y"].toDouble(),
                                   regionObj["w"].toDouble(),
                                   regionObj["h"].toDouble());
            if (regionObj.contains("points") && regionObj["points"].isArray()) {
                for (const auto& pointVal : regionObj["points"].toArray()) {
                    const QJsonObject pointObj = pointVal.toObject();
                    region.points.push_back(QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble()));
                }
            }
            impl_->regions.push_back(region);
        }
    }
    impl_->result.trackerId = impl_->id;
    impl_->result.name = impl_->name;
    
    QJsonArray framesArray = root["frames"].toArray();
    for (const auto& frameVal : framesArray) {
        QJsonObject frameObj = frameVal.toObject();
        TrackFrame frame;
        frame.time = frameObj["time"].toDouble();
        frame.overallConfidence = frameObj["confidence"].toDouble();
        frame.hasHomography = frameObj["hasHomography"].toBool(false);
        if (frame.hasHomography && frameObj["homography"].isArray()) {
            const QJsonArray homographyArray = frameObj["homography"].toArray();
            if (homographyArray.size() == static_cast<int>(frame.homography.size())) {
                for (int index = 0; index < homographyArray.size(); ++index) {
                    frame.homography[static_cast<std::size_t>(index)] =
                        homographyArray[index].toDouble(frame.homography[static_cast<std::size_t>(index)]);
                }
            } else {
                frame.hasHomography = false;
            }
        }
        
        QJsonArray pointsArray = frameObj["points"].toArray();
        for (const auto& pointVal : pointsArray) {
            QJsonObject pointObj = pointVal.toObject();
            TrackPoint p;
            p.id = pointObj["id"].toInt();
            p.position = QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble());
            p.velocity = QPointF(pointObj["vx"].toDouble(), pointObj["vy"].toDouble());
            p.confidence = pointObj["confidence"].toDouble();
            p.active = pointObj["active"].toBool();
            frame.points.push_back(p);
        }
        impl_->result.frames.push_back(std::move(frame));
    }

    if (root.contains("failureFrames") && root["failureFrames"].isArray()) {
        for (const auto& failureVal : root["failureFrames"].toArray()) {
            impl_->result.addFailureFrame(failureVal.toDouble());
        }
    }

    impl_->result.normalize();
    return true;
}
class TrackerManager::Impl {
public:
    QMap<int, MotionTracker*> trackers;
    int nextId = 1;
};

TrackerManager::TrackerManager() : impl_(new Impl()) {}

TrackerManager::~TrackerManager() {
    clearTrackers();
    delete impl_;
}

TrackerManager& TrackerManager::instance() {
    static TrackerManager manager;
    return manager;
}

MotionTracker* TrackerManager::createTracker(const QString& name) {
    MotionTracker* tracker = new MotionTracker();
    tracker->setName(name.isEmpty() ? QString("Tracker %1").arg(impl_->nextId) : name);
    impl_->trackers[tracker->id()] = tracker;
    ++impl_->nextId;
    return tracker;
}

MotionTracker* TrackerManager::tracker(int id) {
    return impl_->trackers.value(id, nullptr);
}

const MotionTracker* TrackerManager::tracker(int id) const {
    return impl_->trackers.value(id, nullptr);
}

void TrackerManager::removeTracker(int id) {
    delete impl_->trackers.take(id);
}

void TrackerManager::clearTrackers() {
    for (auto* tracker : impl_->trackers) {
        delete tracker;
    }
    impl_->trackers.clear();
}

std::vector<MotionTracker*> TrackerManager::allTrackers() {
    auto qlist = impl_->trackers.values();
    std::vector<MotionTracker*> result;
    result.reserve(static_cast<std::size_t>(qlist.size()));
    for (auto* tracker : qlist) {
        result.push_back(tracker);
    }
    return result;
}


int TrackerManager::trackerCount() const {
    return impl_->trackers.size();
}

void TrackerManager::trackAllTrackers(double startTime, double endTime,
                                       std::function<bool(double progress)> progressCallback) {
    int total = trackerCount();
    if (total <= 0) {
        if (progressCallback) {
            progressCallback(1.0);
        }
        return;
    }
    int current = 0;
    
    for (auto* tracker : impl_->trackers) {
        if (!tracker) {
            ++current;
            continue;
        }
        tracker->trackRange(startTime, endTime);
        ++current;
        if (progressCallback) {
            try {
                if (!progressCallback(static_cast<double>(current) / total)) {
                    break;
                }
            } catch (...) {
                break;
            }
        }
    }
}

// ============================================================================
// OpticalFlow ユーティリティ実装
// ============================================================================

namespace OpticalFlow {

std::vector<std::pair<QPointF, QPointF>> computeFlow(
    const QImage& frame1, 
    const QImage& frame2,
    const TrackerSettings& settings) 
{
    std::vector<std::pair<QPointF, QPointF>> flow;
    // 実際の実装ではOpenCVを使用
    Q_UNUSED(frame1);
    Q_UNUSED(frame2);
    Q_UNUSED(settings);
    return flow;
}

QImage computeDenseFlow(const QImage& frame1, const QImage& frame2) {
    // Farnebackアルゴリズム等を使用
    Q_UNUSED(frame1);
    Q_UNUSED(frame2);
    return QImage();
}

QImage visualizeFlow(const std::vector<std::pair<QPointF, QPointF>>& flow, 
                     const QSize& imageSize) 
{
    QImage vis(imageSize, QImage::Format_ARGB32);
    vis.fill(Qt::black);
    
    QPainter painter(&vis);
    painter.setPen(Qt::green);
    
    for (const auto& [start, end] : flow) {
        painter.drawLine(start, start + end);
    }
    return vis;
}

} // namespace OpticalFlow

std::array<double, 9> MotionTracker::computeHomography(
    const std::vector<QPointF>& srcPoints,
    const std::vector<QPointF>& dstPoints) {
    
    if (srcPoints.size() < 4 || dstPoints.size() < 4) {
        return {1, 0, 0, 0, 1, 0, 0, 0, 1};
    }

    std::vector<cv::Point2f> srcFull, dstFull;
    for (size_t i = 0; i < 4; ++i) {
        srcFull.push_back(cv::Point2f(static_cast<float>(srcPoints[i].x()), static_cast<float>(srcPoints[i].y())));
        dstFull.push_back(cv::Point2f(static_cast<float>(dstPoints[i].x()), static_cast<float>(dstPoints[i].y())));
    }

    cv::Mat H = cv::getPerspectiveTransform(srcFull, dstFull);
    std::array<double, 9> resultMat;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            resultMat[i * 3 + j] = H.at<double>(i, j);
        }
    }
    return resultMat;
}

} // namespace ArtifactCore
