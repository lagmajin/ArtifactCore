module;

#include <QString>
#include <QPointF>
#include <QRectF>
#include <QImage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>

module Tracking.MotionTracker;

import std;

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

// ============================================================================
// TrackResult 実装
// ============================================================================

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
            return result;
        }
    }
    return frames.back();
}

QPointF TrackResult::averagePosition(double time) const {
    TrackFrame frame = interpolateAt(time);
    if (frame.points.empty()) return QPointF();
    
    QPointF sum;
    for (const auto& p : frame.points) {
        sum += p.position;
    }
    return sum / frame.points.size();
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

// ============================================================================
// MotionTracker::Impl
// ============================================================================

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
    QMap<double, QImage> frameBuffer;
    double currentTime = 0.0;
    
    // トラッキング結果
    TrackResult result;
    
    // 状態
    bool isTracking = false;
    bool shouldStop = false;
    
    int nextPointId = 1;
    int nextRegionId = 1;
    
    // オプティカルフロー計算（簡易実装）
    QPointF computeOpticalFlow(const QImage& prev, const QImage& curr, const QPointF& point) {
        // 簡易的な実装（実際はOpenCVなどを使用）
        Q_UNUSED(prev);
        Q_UNUSED(curr);
        Q_UNUSED(point);
        return QPointF(0, 0);
    }
    
    // 特徴点検出
    std::vector<QPointF> detectFeatures(const QImage& frame, int maxFeatures, double minDist) {
        std::vector<QPointF> features;
        // 簡易実装（実際はOpenCVのgoodFeaturesToTrackなど）
        Q_UNUSED(frame);
        Q_UNUSED(maxFeatures);
        Q_UNUSED(minDist);
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
}

TrackerSettings MotionTracker::settings() const {
    return impl_->settings;
}

void MotionTracker::setTrackerType(TrackerType type) {
    impl_->type = type;
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
    impl_->frameBuffer[time] = frame;
    impl_->currentTime = time;
}

bool MotionTracker::trackForward(double fromTime, double toTime) {
    auto it1 = impl_->frameBuffer.find(fromTime);
    auto it2 = impl_->frameBuffer.find(toTime);
    
    if (it1 == impl_->frameBuffer.end() || it2 == impl_->frameBuffer.end()) {
        return false;
    }
    
    // オプティカルフロー計算
    for (auto& point : impl_->currentPoints) {
        QPointF flow = impl_->computeOpticalFlow(it1.value(), it2.value(), point.position);
        point.position += flow;
    }
    
    // 結果保存
    TrackFrame frame;
    frame.time = toTime;
    frame.points = impl_->currentPoints;
    impl_->result.frames.push_back(frame);
    
    return true;
}

bool MotionTracker::trackBackward(double fromTime, double toTime) {
    auto it1 = impl_->frameBuffer.find(fromTime);
    auto it2 = impl_->frameBuffer.find(toTime);
    
    if (it1 == impl_->frameBuffer.end() || it2 == impl_->frameBuffer.end()) {
        return false;
    }
    
    // 逆方向トラッキング
    for (auto& point : impl_->currentPoints) {
        QPointF flow = impl_->computeOpticalFlow(it1.value(), it2.value(), point.position);
        point.position -= flow;
    }
    
    TrackFrame frame;
    frame.time = toTime;
    frame.points = impl_->currentPoints;
    impl_->result.frames.insert(impl_->result.frames.begin(), frame);
    
    return true;
}

bool MotionTracker::trackRange(double startTime, double endTime, 
                               std::function<bool(double progress)> progressCallback) {
    impl_->isTracking = true;
    impl_->shouldStop = false;
    
    // 時間順にソート
    QList<double> times = impl_->frameBuffer.keys();
    std::sort(times.begin(), times.end());
    
    int totalSteps = 0;
    int currentStep = 0;
    
    for (double t : times) {
        if (t >= startTime && t <= endTime) totalSteps++;
    }
    
    for (double t : times) {
        if (impl_->shouldStop) break;
        if (t < startTime || t > endTime) continue;
        
        if (t > startTime) {
            double prevTime = times.at(times.indexOf(t) - 1);
            trackForward(prevTime, t);
        }
        
        currentStep++;
        if (progressCallback) {
            if (!progressCallback(static_cast<double>(currentStep) / totalSteps)) {
                break;
            }
        }
    }
    
    impl_->result.startTime = startTime;
    impl_->result.endTime = endTime;
    impl_->result.isValid = !impl_->result.frames.empty();
    impl_->isTracking = false;
    
    return impl_->result.isValid;
}

bool MotionTracker::trackAll(std::function<bool(double progress)> progressCallback) {
    if (impl_->frameBuffer.isEmpty()) return false;
    
    QList<double> times = impl_->frameBuffer.keys();
    return trackRange(times.first(), times.last(), progressCallback);
}

void MotionTracker::stopTracking() {
    impl_->shouldStop = true;
}

void MotionTracker::resetTracking() {
    impl_->result = TrackResult();
    impl_->result.trackerId = impl_->id;
    impl_->result.name = impl_->name;
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
    // 簡易実装：2点間の角度から回転を推定
    Q_UNUSED(time);
    return 0.0;
}

QPointF MotionTracker::scaleAt(double time) const {
    // 簡易実装：2点間の距離変化からスケールを推定
    Q_UNUSED(time);
    return QPointF(1.0, 1.0);
}

std::vector<QPointF> MotionTracker::motionPath(int pointId) const {
    return impl_->result.motionPath(pointId);
}

std::vector<std::pair<double, QPointF>> MotionTracker::exportKeyframes(int pointId) const {
    std::vector<std::pair<double, QPointF>> keyframes;
    for (const auto& frame : impl_->result.frames) {
        const TrackPoint* p = frame.findPoint(pointId);
        if (p) {
            keyframes.push_back({frame.time, p->position});
        }
    }
    return keyframes;
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
    for (const auto& frame : impl_->result.frames) {
        if (frame.overallConfidence < impl_->settings.confidenceThreshold) {
            problems.push_back(frame.time);
        }
    }
    return problems;
}

QPointF MotionTracker::averageVelocity() const {
    if (impl_->result.frames.size() < 2) return QPointF();
    
    QPointF totalVelocity;
    int count = 0;
    for (const auto& frame : impl_->result.frames) {
        for (const auto& p : frame.points) {
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
        for (auto& p : frame.points) {
            if (p.confidence < threshold) {
                p.active = false;
            }
        }
    }
}

void MotionTracker::smoothTrack(int windowSize) {
    if (impl_->result.frames.size() < static_cast<size_t>(windowSize)) return;
    
    // 移動平均フィルタ
    int halfWindow = windowSize / 2;
    std::vector<TrackFrame> smoothed = impl_->result.frames;
    
    for (size_t i = halfWindow; i < impl_->result.frames.size() - halfWindow; ++i) {
        for (auto& point : smoothed[i].points) {
            QPointF sum;
            int count = 0;
            for (int j = -halfWindow; j <= halfWindow; ++j) {
                const TrackPoint* p = impl_->result.frames[i + j].findPoint(point.id);
                if (p) {
                    sum += p->position;
                    ++count;
                }
            }
            if (count > 0) {
                point.position = sum / count;
            }
        }
    }
    
    impl_->result.frames = smoothed;
}

void MotionTracker::removeOutliers(double threshold) {
    // 統計的な外れ値除去
    for (auto& frame : impl_->result.frames) {
        for (auto& p : frame.points) {
            // 速度から外れ値判定
            double speed = std::sqrt(p.velocity.x() * p.velocity.x() + 
                                    p.velocity.y() * p.velocity.y());
            if (speed > threshold) {
                p.active = false;
            }
        }
    }
}

void MotionTracker::applyCorrection(double time, int pointId, const QPointF& correctedPosition) {
    for (auto& frame : impl_->result.frames) {
        if (std::abs(frame.time - time) < 0.001) {
            TrackPoint* p = frame.findPoint(pointId);
            if (p) {
                p->position = correctedPosition;
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
    root["id"] = impl_->id;
    root["name"] = impl_->name;
    
    QJsonArray framesArray;
    for (const auto& frame : impl_->result.frames) {
        QJsonObject frameObj;
        frameObj["time"] = frame.time;
        frameObj["confidence"] = frame.overallConfidence;
        
        QJsonArray pointsArray;
        for (const auto& p : frame.points) {
            QJsonObject pointObj;
            pointObj["id"] = p.id;
            pointObj["x"] = p.position.x();
            pointObj["y"] = p.position.y();
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
    if (doc.isNull()) return false;
    
    QJsonObject root = doc.object();
    impl_->id = root["id"].toInt();
    impl_->name = root["name"].toString();
    
    impl_->result = TrackResult();
    impl_->result.trackerId = impl_->id;
    impl_->result.name = impl_->name;
    
    QJsonArray framesArray = root["frames"].toArray();
    for (const auto& frameVal : framesArray) {
        QJsonObject frameObj = frameVal.toObject();
        TrackFrame frame;
        frame.time = frameObj["time"].toDouble();
        frame.overallConfidence = frameObj["confidence"].toDouble();
        
        QJsonArray pointsArray = frameObj["points"].toArray();
        for (const auto& pointVal : pointsArray) {
            QJsonObject pointObj = pointVal.toObject();
            TrackPoint p;
            p.id = pointObj["id"].toInt();
            p.position = QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble());
            p.confidence = pointObj["confidence"].toDouble();
            p.active = pointObj["active"].toBool();
            frame.points.push_back(p);
        }
        impl_->result.frames.push_back(frame);
    }
    
    impl_->result.isValid = !impl_->result.frames.empty();
    return true;
}

// ============================================================================
// TrackerManager 実装
// ============================================================================

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
    return impl_->trackers.values().toVector().toStdVector();
}

int TrackerManager::trackerCount() const {
    return impl_->trackers.size();
}

void TrackerManager::trackAllTrackers(double startTime, double endTime,
                                       std::function<bool(double progress)> progressCallback) {
    int total = trackerCount();
    int current = 0;
    
    for (auto* tracker : impl_->trackers) {
        tracker->trackRange(startTime, endTime);
        ++current;
        if (progressCallback) {
            if (!progressCallback(static_cast<double>(current) / total)) {
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

} // namespace ArtifactCore