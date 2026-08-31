module;
#include <utility>
#include <QImage>
#include <QRect>
#include <QVector>
//#include <opencv2/tracking.hpp>
#include <memory>
#include <vector>
#include <map>

export module ArtifactCore.ImageProcessing.FaceTracker;

import ArtifactCore.ImageProcessing.FaceDetection;

export namespace ArtifactCore {

// 追従中の顔の情報
struct TrackedFace {
    int trackId = -1;
    QRect rect;
    float confidence = 0.0f;
    int framesLost = 0;      // 検出ロストしたフレーム数
    int age = 0;             // 追従開始からのフレーム数
    QRect prevRect;          // 前フレームの矩形
};

// 追従設定
struct FaceTrackerSettings {
    int maxLostFrames = 10;     // 何フレームロストしたら追従終了するか
    float smoothFactor = 0.3f;  // スムージング係数 (0.0 = 前フレーム優先, 1.0 = 現在優先)
    bool useSmoothing = true;   // 位置スムージングを有効にする
    int minDetectionInterval = 3; // 何フレームごとに再検出するか
};

// 顔追従エンジン
class FaceTracker {
public:
    FaceTracker();
    ~FaceTracker();

    // 初期化
    void setSettings(const FaceTrackerSettings& settings);
    const FaceTrackerSettings& settings() const;

    // フレーム更新
    // 検出結果を受け取り、追従IDを割り当てて返す
    QVector<TrackedFace> update(const QVector<FaceDetectionResult>& detections);

    // リセット（全追従クリア）
    void reset();

    // 現在の追従状態
    int activeTrackCount() const;
    const QVector<TrackedFace>& trackedFaces() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
