module;
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>
#include <QString>
#include <QImage>

export module ImageProcessing.ColorTransform.FilmCurve;

export namespace ArtifactCore {

/// <summary>
/// フィルムカーブ（Toe / Shoulder 制御）
/// 
/// フィルムの特性曲線を模倣し、ハイライトの粘りやシャドウの潰れを制御する。
/// 単なるS字カーブではなく、フィルム特有の非線形応答を再現する。
/// </summary>

/// フィルムカーブ設定
struct FilmCurveSettings {
    // Toe（シャドウ側）
    float toeStrength = 0.0f;     // 0-1: シャドウの潰れ具合
    float toeStart = 0.0f;        // 0-1: Toe開始位置（低いほど暗部から）
    float toeSoftness = 0.5f;     // 0-1: Toeの柔らかさ

    // Shoulder（ハイライト側）
    float shoulderStrength = 0.0f;  // 0-1: ハイライトの粘り
    float shoulderStart = 1.0f;     // 0-1: Shoulder開始位置（高いほど亮部から）
    float shoulderSoftness = 0.5f;  // 0-1: Shoulderの柔らかさ

    // 中間調
    float midtoneContrast = 1.0f;   // 0-2: 中間調のコントラスト
    float midtoneBias = 0.5f;       // 0-1: 中間調の位置

    // 出力
    float outputBlack = 0.0f;       // 0-1: 出力黒レベル
    float outputWhite = 1.0f;       // 0-1: 出力白レベル

    /// リセット（リニアな応答に戻す）
    void reset() {
        toeStrength = 0.0f;
        toeStart = 0.0f;
        toeSoftness = 0.5f;
        shoulderStrength = 0.0f;
        shoulderStart = 1.0f;
        shoulderSoftness = 0.5f;
        midtoneContrast = 1.0f;
        midtoneBias = 0.5f;
        outputBlack = 0.0f;
        outputWhite = 1.0f;
    }

    /// プリセット: フィルムライク
    static FilmCurveSettings filmic() {
        FilmCurveSettings settings;
        settings.toeStrength = 0.15f;
        settings.toeStart = 0.05f;
        settings.toeSoftness = 0.6f;
        settings.shoulderStrength = 0.2f;
        settings.shoulderStart = 0.85f;
        settings.shoulderSoftness = 0.5f;
        settings.midtoneContrast = 1.1f;
        return settings;
    }

    /// プリセット: ビンテージフィルム
    static FilmCurveSettings vintageFilm() {
        FilmCurveSettings settings;
        settings.toeStrength = 0.25f;
        settings.toeStart = 0.1f;
        settings.toeSoftness = 0.7f;
        settings.shoulderStrength = 0.3f;
        settings.shoulderStart = 0.75f;
        settings.shoulderSoftness = 0.6f;
        settings.midtoneContrast = 0.9f;
        settings.outputBlack = 0.02f;
        settings.outputWhite = 0.95f;
        return settings;
    }

    /// プリセット: ハイコントラスト
    static FilmCurveSettings highContrast() {
        FilmCurveSettings settings;
        settings.toeStrength = 0.1f;
        settings.shoulderStrength = 0.15f;
        settings.midtoneContrast = 1.3f;
        return settings;
    }
};

/// フィルムカーブプロセッサ
class FilmCurveProcessor {
public:
    FilmCurveProcessor();
    ~FilmCurveProcessor();

    /// 設定
    void setSettings(const FilmCurveSettings& settings);
    auto settings() const -> const FilmCurveSettings&;

    /// 画像に適用
    auto apply(const QImage& source) const -> QImage;

    /// 単一チャネル値に変換を適用（0-1範囲）
    auto applyChannel(float value) const -> float;

    /// ルミナンスに適用
    void applyToLuma(float& r, float& g, float& b) const;

private:
    FilmCurveSettings settings_;

    /// Toe関数
    auto applyToe(float value) const -> float;

    /// Shoulder関数
    auto applyShoulder(float value) const -> float;

    /// 中間調コントラスト
    auto applyMidtone(float value) const -> float;

    /// 滑らかなクリンプ関数
    auto smoothClamp(float value, float start, float strength, float softness) const -> float;
};

} // namespace ArtifactCore
