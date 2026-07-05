module;
#include <utility>

#include <QString>
#include <QPointF>
#include <QRectF>
#include <QImage>
#include <vector>
#include <array>
#include <memory>
#include <functional>

export module Tracking.MotionTracker;

export namespace ArtifactCore {

/// トラッキング方法
enum class TrackingMethod {
    OpticalFlow,    ///< オプティカルフロー（高精度）
    FeatureBased,   ///< 特徴点ベース（高速）
    TemplateMatch,  ///< テンプレートマッチング
    Hybrid          ///< ハイブリッド
};

/// トラッキング品質
enum class TrackingQuality {
    Fast,       ///< 高速・低精度
    Normal,     ///< 標準
    High,       ///< 高精度
    Ultra       ///< 最高精度
};

/// トラッカータイプ
enum class TrackerType {
    Point,      ///< 点トラッカー
    Planar,     ///< 平面トラッカー
    Spline,     ///< スプライントラッカー
    Perspective ///< パースペクティブトラッカー
};

/// トラッキングポイント
struct TrackPoint {
    int id;                 ///< ポイントID
    QPointF position;       ///< 現在位置
    QPointF velocity;       ///< 速度ベクトル
    double confidence;      ///< 信頼度 (0-1)
    bool active = true;     ///< アクティブかどうか
    
    TrackPoint() = default;
    TrackPoint(int id_, const QPointF& pos) : id(id_), position(pos), confidence(1.0) {}
};

/// トラッキングデータ（1フレーム分）
struct TrackFrame {
    double time = 0.0;                  ///< 時間（秒）
    std::vector<TrackPoint> points;     ///< トラッキングポイント
    std::array<double, 9> homography = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    bool hasHomography = false;
    double overallConfidence = 1.0;     ///< 全体の信頼度
    
    TrackPoint* findPoint(int id);
    const TrackPoint* findPoint(int id) const;
    void sortPointsById();
};

/// トラッキング結果
struct TrackResult {
    int schemaVersion = 1;              ///< 結果スキーマ版本
    int trackerId = 0;                  ///< トラッカーID
    QString name;                       ///< 名前
    QString sourceName;                 ///< 元ソース名
    QString sourcePath;                 ///< 元ソースパス
    QString sourceType;                 ///< 元ソース種別
    std::vector<TrackFrame> frames;     ///< フレームごとのデータ
    std::vector<double> failureFrames;  ///< 失敗したフレーム時刻
    double startTime = 0.0;             ///< 開始時間
    double endTime = 0.0;               ///< 終了時間
    bool isValid = false;               ///< 有効かどうか
    
    /// 指定時間の補間データを取得
    TrackFrame interpolateAt(double time) const;
    
    /// 全ポイントの平均位置を取得
    QPointF averagePosition(double time) const;
    
    /// モーションパスを取得
    std::vector<QPointF> motionPath(int pointId) const;

    /// フレームを時間順・ポイントID順に整える
    void normalize();
    
    /// フレームを追加または置換する
    void setFrame(TrackFrame frame);

    /// 失敗フレームを記録する
    void addFailureFrame(double time);

    /// 失敗フレーム記録を消去する
    void clearFailureFrames();
    
    /// フレーム数取得
    size_t frameCount() const;
};

/// トラッカー設定
struct TrackerSettings {
    TrackingMethod method = TrackingMethod::OpticalFlow;
    TrackingQuality quality = TrackingQuality::Normal;
    TrackerType type = TrackerType::Point;
    
    // 検出パラメータ
    int maxFeatures = 100;          ///< 最大特徴点数
    double minDistance = 10.0;      ///< 特徴点間の最小距離
    int windowSize = 21;            ///< 追跡ウィンドウサイズ
    int maxPyramidLevel = 3;        ///< ピラミッドレベル数
    
    // 閾値
    double confidenceThreshold = 0.5;   ///< 信頼度閾値
    double errorThreshold = 10.0;       ///< エラー閾値（ピクセル）
    
    // 前後トラッキング
    bool trackForward = true;
    bool trackBackward = false;
    
    // サブピクセル精度
    bool subpixelAccuracy = true;
    int subpixelIterations = 10;
    
    // 検索領域
    QRectF searchRegion;            ///< 検索領域（空なら全体）
    
    /// デフォルト設定
    static TrackerSettings fast();
    static TrackerSettings normal();
    static TrackerSettings highQuality();
};

/// トラッキング領域
struct TrackRegion {
    int id;
    QRectF bounds;                  ///< バウンディングボックス
    std::vector<QPointF> points;    ///< 内部のトラッキングポイント
    QPointF center() const;
};

/// モーショントラッカー
class MotionTracker {
public:
    MotionTracker();
    ~MotionTracker();
    
    MotionTracker(const MotionTracker& other);
    MotionTracker& operator=(const MotionTracker& other);
    MotionTracker(MotionTracker&& other) noexcept;
    MotionTracker& operator=(MotionTracker&& other) noexcept;
    
    // 設定
    void setSettings(const TrackerSettings& settings);
    TrackerSettings settings() const;
    void setTrackerType(TrackerType type);
    TrackerType trackerType() const;
    void setName(const QString& name);
    QString name() const;
    int id() const;
    
    // トラッキング領域設定
    int addTrackPoint(const QPointF& point);
    void addTrackPoints(const std::vector<QPointF>& points);
    int addTrackRegion(const QRectF& bounds);
    void removeTrackPoint(int pointId);
    void removeTrackRegion(int regionId);
    void clearTrackPoints();
    void clearTrackRegions();
    int trackPointCount() const;
    int trackRegionCount() const;
    TrackPoint trackPoint(int index) const;
    std::vector<TrackPoint> trackPoints() const;
    
    // トラッキング実行
    void setFrame(double time, const QImage& frame);
    bool trackForward(double fromTime, double toTime);
    bool trackBackward(double fromTime, double toTime);
    bool trackRange(double startTime, double endTime, 
                    std::function<bool(double progress)> progressCallback = nullptr);
    bool trackAll(std::function<bool(double progress)> progressCallback = nullptr);
    void stopTracking();
    void resetTracking();
    void clearTrackingData();
    
    // 結果取得
    TrackResult result() const;
    QPointF pointPositionAt(int pointId, double time) const;
    std::vector<QPointF> allPointPositionsAt(double time) const;
    QPointF displacementAt(double time) const;
    double rotationAt(double time) const;
    QPointF scaleAt(double time) const;
    std::vector<QPointF> motionPath(int pointId) const;
    std::vector<std::pair<double, QPointF>> exportKeyframes(int pointId) const;
    bool hasResult() const;
    
    // 統計・解析
    double averageConfidence() const;
    double qualityScore() const;
    std::vector<double> problemFrames() const;
    QPointF averageVelocity() const;
    double totalDistance(int pointId) const;
    
    // 補正
    void filterByConfidence(double threshold);
    void smoothTrack(int windowSize = 5);
    void removeOutliers(double threshold = 3.0);
    void applyCorrection(double time, int pointId, const QPointF& correctedPosition);
    
    // ユーティリティ
    static std::array<double, 9> computeHomography(
        const std::vector<QPointF>& srcPoints,
        const std::vector<QPointF>& dstPoints);

    // シリアライズ
    bool saveToFile(const QString& filePath) const;
    bool loadFromFile(const QString& filePath);
    QString toJson() const;
    bool fromJson(const QString& json);

private:
    class Impl;
    Impl* impl_;
};

/// トラッカーマネージャー
class TrackerManager {
public:
    static TrackerManager& instance();
    MotionTracker* createTracker(const QString& name = "");
    MotionTracker* tracker(int id);
    const MotionTracker* tracker(int id) const;
    void removeTracker(int id);
    void clearTrackers();
    std::vector<MotionTracker*> allTrackers();
    int trackerCount() const;
    void trackAllTrackers(double startTime, double endTime,
                          std::function<bool(double progress)> progressCallback = nullptr);
private:
    TrackerManager();
    ~TrackerManager();
    class Impl;
    Impl* impl_;
};

namespace OpticalFlow {
    std::vector<std::pair<QPointF, QPointF>> computeFlow(
        const QImage& frame1, 
        const QImage& frame2,
        const TrackerSettings& settings = TrackerSettings::normal()
    );
    QImage computeDenseFlow(const QImage& frame1, const QImage& frame2);
    QImage visualizeFlow(const std::vector<std::pair<QPointF, QPointF>>& flow, 
                         const QSize& imageSize);
}

} // namespace ArtifactCore
