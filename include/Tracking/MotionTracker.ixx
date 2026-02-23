module;

#include <QString>
#include <QPointF>
#include <QRectF>
#include <QImage>
#include <vector>

export module Tracking.MotionTracker;

import std;

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
    double time;                        ///< 時間（秒）
    std::vector<TrackPoint> points;     ///< トラッキングポイント
    double overallConfidence = 1.0;     ///< 全体の信頼度
    
    TrackPoint* findPoint(int id);
    const TrackPoint* findPoint(int id) const;
};

/// トラッキング結果
struct TrackResult {
    int trackerId;                      ///< トラッカーID
    QString name;                       ///< 名前
    std::vector<TrackFrame> frames;     ///< フレームごとのデータ
    double startTime = 0.0;             ///< 開始時間
    double endTime = 0.0;               ///< 終了時間
    bool isValid = false;               ///< 有効かどうか
    
    /// 指定時間の補間データを取得
    TrackFrame interpolateAt(double time) const;
    
    /// 全ポイントの平均位置を取得
    QPointF averagePosition(double time) const;
    
    /// モーションパスを取得
    std::vector<QPointF> motionPath(int pointId) const;
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
/// 
/// 動画内のオブジェクトを追跡する機能を提供。
/// オプティカルフロー、特徴点追跡、テンプレートマッチングに対応。
class MotionTracker {
public:
    MotionTracker();
    ~MotionTracker();
    
    MotionTracker(const MotionTracker& other);
    MotionTracker& operator=(const MotionTracker& other);
    MotionTracker(MotionTracker&& other) noexcept;
    MotionTracker& operator=(MotionTracker&& other) noexcept;
    
    // ========================================
    // 設定
    // ========================================
    
    /// トラッカー設定
    void setSettings(const TrackerSettings& settings);
    TrackerSettings settings() const;
    
    /// トラッカータイプ
    void setTrackerType(TrackerType type);
    TrackerType trackerType() const;
    
    /// トラッカー名
    void setName(const QString& name);
    QString name() const;
    
    /// トラッカーID
    int id() const;
    
    // ========================================
    // トラッキング領域設定
    // ========================================
    
    /// トラッキングポイントを追加
    int addTrackPoint(const QPointF& point);
    
    /// 複数ポイントを追加
    void addTrackPoints(const std::vector<QPointF>& points);
    
    /// トラッキング領域を追加（平面トラッキング用）
    int addTrackRegion(const QRectF& bounds);
    
    /// ポイントを削除
    void removeTrackPoint(int pointId);
    
    /// 領域を削除
    void removeTrackRegion(int regionId);
    
    /// 全ポイントをクリア
    void clearTrackPoints();
    
    /// 全領域をクリア
    void clearTrackRegions();
    
    /// ポイント数取得
    int trackPointCount() const;
    
    /// 領域数取得
    int trackRegionCount() const;
    
    /// ポイント取得
    TrackPoint trackPoint(int index) const;
    
    /// 全ポイント取得
    std::vector<TrackPoint> trackPoints() const;
    
    // ========================================
    // トラッキング実行
    // ========================================
    
    /// フレームを設定
    void setFrame(double time, const QImage& frame);
    
    /// 単一フレームのトラッキング（順方向）
    bool trackForward(double fromTime, double toTime);
    
    /// 単一フレームのトラッキング（逆方向）
    bool trackBackward(double fromTime, double toTime);
    
    /// 範囲トラッキング
    bool trackRange(double startTime, double endTime, 
                    std::function<bool(double progress)> progressCallback = nullptr);
    
    /// 全範囲トラッキング
    bool trackAll(std::function<bool(double progress)> progressCallback = nullptr);
    
    /// トラッキング停止
    void stopTracking();
    
    /// トラッキングリセット
    void resetTracking();
    
    // ========================================
    // 結果取得
    // ========================================
    
    /// トラッキング結果取得
    TrackResult result() const;
    
    /// 指定時間のポイント位置取得
    QPointF pointPositionAt(int pointId, double time) const;
    
    /// 指定時間の全ポイント位置取得
    std::vector<QPointF> allPointPositionsAt(double time) const;
    
    /// 指定時間の変位（オフセット）取得
    QPointF displacementAt(double time) const;
    
    /// 指定時間の回転取得
    double rotationAt(double time) const;
    
    /// 指定時間のスケール取得
    QPointF scaleAt(double time) const;
    
    /// モーションパス取得
    std::vector<QPointF> motionPath(int pointId) const;
    
    /// キーフレームとしてエクスポート
    std::vector<std::pair<double, QPointF>> exportKeyframes(int pointId) const;
    
    // ========================================
    // 統計・解析
    // ========================================
    
    /// 平均信頼度
    double averageConfidence() const;
    
    /// トラッキング品質評価
    double qualityScore() const;
    
    /// 問題のあるフレーム検出
    std::vector<double> problemFrames() const;
    
    /// 平均速度
    QPointF averageVelocity() const;
    
    /// 全移動距離
    double totalDistance(int pointId) const;
    
    // ========================================
    // 補正
    // ========================================
    
    /// 信頼度でフィルタリング
    void filterByConfidence(double threshold);
    
    /// スムージング
    void smoothTrack(int windowSize = 5);
    
    /// 外れ値除去
    void removeOutliers(double threshold = 3.0);
    
    /// 手動補正適用
    void applyCorrection(double time, int pointId, const QPointF& correctedPosition);
    
    // ========================================
    // シリアライズ
    // ========================================
    
    /// 結果をファイルに保存
    bool saveToFile(const QString& filePath) const;
    
    /// ファイルから読み込み
    bool loadFromFile(const QString& filePath);
    
    /// JSONにエクスポート
    QString toJson() const;
    
    /// JSONからインポート
    bool fromJson(const QString& json);

private:
    class Impl;
    Impl* impl_;
};

/// トラッカーマネージャー（複数トラッカー管理）
class TrackerManager {
public:
    static TrackerManager& instance();
    
    /// トラッカー作成
    MotionTracker* createTracker(const QString& name = "");
    
    /// トラッカー取得
    MotionTracker* tracker(int id);
    const MotionTracker* tracker(int id) const;
    
    /// トラッカー削除
    void removeTracker(int id);
    
    /// 全トラッカー削除
    void clearTrackers();
    
    /// 全トラッカー取得
    std::vector<MotionTracker*> allTrackers();
    
    /// トラッカー数
    int trackerCount() const;
    
    /// 一括トラッキング実行
    void trackAllTrackers(double startTime, double endTime,
                          std::function<bool(double progress)> progressCallback = nullptr);

private:
    TrackerManager();
    ~TrackerManager();
    
    class Impl;
    Impl* impl_;
};

/// ユーティリティ：オプティカルフロー計算
namespace OpticalFlow {
    /// 2フレーム間のオプティカルフロー計算
    std::vector<std::pair<QPointF, QPointF>> computeFlow(
        const QImage& frame1, 
        const QImage& frame2,
        const TrackerSettings& settings = TrackerSettings::normal()
    );
    
    /// 密なオプティカルフロー（Farneback）
    QImage computeDenseFlow(const QImage& frame1, const QImage& frame2);
    
    /// モーションベクトルの可視化
    QImage visualizeFlow(const std::vector<std::pair<QPointF, QPointF>>& flow, 
                         const QSize& imageSize);
}

} // namespace ArtifactCore