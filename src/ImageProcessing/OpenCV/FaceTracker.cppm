module;
#include <QImage>
#include <QRect>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

module ArtifactCore.ImageProcessing.FaceTracker;

import ArtifactCore.ImageProcessing.FaceDetection;

namespace ArtifactCore {

// 矩形の中心と面積
static QPointF rectCenter(const QRect& r) {
    return QPointF(r.x() + r.width() * 0.5f, r.y() + r.height() * 0.5f);
}

static float rectArea(const QRect& r) {
    return static_cast<float>(r.width() * r.height());
}

// 2点間の距離
static float pointDistance(const QPointF& a, const QPointF& b) {
    float dx = a.x() - b.x();
    float dy = a.y() - b.y();
    return std::sqrt(dx * dx + dy * dy);
}

// 矩形のオーバーラップ率 (IoU)
static float intersectionOverUnion(const QRect& a, const QRect& b) {
    QRect intersection = a.intersected(b);
    if (intersection.isEmpty()) return 0.0f;
    float interArea = static_cast<float>(intersection.width() * intersection.height());
    float unionArea = rectArea(a) + rectArea(b) - interArea;
    return unionArea > 0 ? interArea / unionArea : 0.0f;
}

class FaceTracker::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    FaceTrackerSettings settings_;
    QVector<TrackedFace> trackedFaces_;
    int nextTrackId_ = 1;
    int frameCount_ = 0;

    // 前フレームの追従状態を保持（スムージング用）
    std::map<int, QRect> prevRects_;

    QVector<TrackedFace> updateFaces(const QVector<FaceDetectionResult>& detections) {
        frameCount_++;

        // 現在の検出と既存の追従をマッチング
        std::vector<bool> detectionUsed(detections.size(), false);
        std::vector<bool> trackLost(trackedFaces_.size(), false);

        // 各追従顔に対して、最も近い検出結果をマッチ
        for (int t = 0; t < trackedFaces_.size(); ++t) {
            TrackedFace& track = trackedFaces_[t];
            int bestIdx = -1;
            float bestScore = 0.0f;

            for (int d = 0; d < detections.size(); ++d) {
                if (detectionUsed[d]) continue;

                float iou = intersectionOverUnion(track.rect, detections[d].rect);
                float centerDist = pointDistance(rectCenter(track.rect), rectCenter(detections[d].rect));
                float sizeRatio = std::min(static_cast<float>(track.rect.width()), static_cast<float>(detections[d].rect.width())) /
                                  std::max(static_cast<float>(track.rect.width()), static_cast<float>(detections[d].rect.width()));

                // スコア: IoU + 距離 + サイズ比
                float score = iou * 0.5f + (1.0f - std::min(centerDist / 200.0f, 1.0f)) * 0.3f + sizeRatio * 0.2f;

                if (score > bestScore && score > 0.3f) {
                    bestScore = score;
                    bestIdx = d;
                }
            }

            if (bestIdx >= 0) {
                // マッチ成功
                detectionUsed[bestIdx] = true;
                trackLost[t] = false;

                const FaceDetectionResult& det = detections[bestIdx];
                track.prevRect = track.rect;

                // スムージング
                if (settings_.useSmoothing) {
                    float sf = settings_.smoothFactor;
                    float newX = track.rect.x() * (1.0f - sf) + det.rect.x() * sf;
                    float newY = track.rect.y() * (1.0f - sf) + det.rect.y() * sf;
                    float newW = track.rect.width() * (1.0f - sf) + det.rect.width() * sf;
                    float newH = track.rect.height() * (1.0f - sf) + det.rect.height() * sf;
                    track.rect = QRect(
                        static_cast<int>(std::round(newX)),
                        static_cast<int>(std::round(newY)),
                        static_cast<int>(std::round(newW)),
                        static_cast<int>(std::round(newH))
                    );
                } else {
                    track.rect = det.rect;
                }

                track.confidence = det.confidence;
                track.framesLost = 0;
                track.age++;
            } else {
                // マッチ失敗
                trackLost[t] = true;
                track.framesLost++;
                track.age++;
            }
        }

        // ロストした追従を削除
        QVector<TrackedFace> survivingTracks;
        for (int t = 0; t < trackedFaces_.size(); ++t) {
            if (!trackLost[t] || trackedFaces_[t].framesLost < settings_.maxLostFrames) {
                survivingTracks.append(trackedFaces_[t]);
            }
        }
        trackedFaces_ = survivingTracks;

        // 未マッチの検出を新規追従として追加
        for (int d = 0; d < detections.size(); ++d) {
            if (!detectionUsed[d]) {
                TrackedFace newTrack;
                newTrack.trackId = nextTrackId_++;
                newTrack.rect = detections[d].rect;
                newTrack.confidence = detections[d].confidence;
                newTrack.framesLost = 0;
                newTrack.age = 1;
                trackedFaces_.append(newTrack);
            }
        }

        return trackedFaces_;
    }

    void resetFaces() {
        trackedFaces_.clear();
        nextTrackId_ = 1;
        frameCount_ = 0;
        prevRects_.clear();
    }
};

FaceTracker::FaceTracker() : impl_(std::make_unique<Impl>()) {}

FaceTracker::~FaceTracker() = default;

void FaceTracker::setSettings(const FaceTrackerSettings& settings) {
    impl_->settings_ = settings;
}

const FaceTrackerSettings& FaceTracker::settings() const {
    return impl_->settings_;
}

QVector<TrackedFace> FaceTracker::update(const QVector<FaceDetectionResult>& detections) {
    return impl_->updateFaces(detections);
}

void FaceTracker::reset() {
    impl_->resetFaces();
}

int FaceTracker::activeTrackCount() const {
    return impl_->trackedFaces_.size();
}

const QVector<TrackedFace>& FaceTracker::trackedFaces() const {
    return impl_->trackedFaces_;
}

} // namespace ArtifactCore
