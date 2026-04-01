module;
#include <QImage>
#include <QRect>
#include <QVector>
#include <QString>
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <memory>
#include <vector>

export module ArtifactCore.ImageProcessing.FaceDetection;

export namespace ArtifactCore {

// 顔検出結果
struct FaceDetectionResult {
    QRect rect;              // 顔の矩形 (ピクセル座標)
    float confidence = 0.0f; // 信頼度 (0.0 - 1.0)
    int trackId = -1;        // トラッキングID (追従時)
};

// 検出方法
enum class FaceDetectionMethod {
    HaarCascade,  // Haar Cascade (高速)
    DNN,          // OpenCV DNN (高精度)
    Auto          // 自動選択 (プレビュー=Haar, レンダリング=DNN)
};

// 検出設定
struct FaceDetectionSettings {
    FaceDetectionMethod method = FaceDetectionMethod::HaarCascade;
    float scaleFactor = 1.1f;
    int minNeighbors = 3;
    QSize minSize = {30, 30};
    QSize maxSize = {0, 0}; // 0 = 制限なし
    bool useDNNForRender = true;
    int dnnWidth = 320;  // DNN 入力幅
    int dnnHeight = 320; // DNN 入力高さ
};

// 顔検出エンジン
class FaceDetectionEngine {
public:
    FaceDetectionEngine();
    ~FaceDetectionEngine();

    // 初期化
    bool initialize(const FaceDetectionSettings& settings = {});
    bool isInitialized() const;

    // 設定
    void setSettings(const FaceDetectionSettings& settings);
    const FaceDetectionSettings& settings() const;

    // 検出実行
    QVector<FaceDetectionResult> detect(const QImage& image);
    QVector<FaceDetectionResult> detect(const cv::Mat& image);

    // カスケード/DNN モデルのパス設定
    void setHaarCascadePath(const QString& path);
    void setDNNModelPath(const QString& modelPath);
    void setDNNConfigPath(const QString& configPath);

    // エラー情報
    QString lastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
