module;
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>
#include <QString>
#include <QImage>

export module ImageProcessing.ColorTransform.TonalSplit;

export namespace ArtifactCore {

/// <summary>
/// トーン分離（Tonal Split）
/// 
/// ハイライトとシャドウを別色に分離し、映像の「空気感」を作る。
/// LUTより軽くて直感的。AEは弱い分野だが、映像系では常識。
/// 
/// 例:
/// - シャドウを青緑に、ハイライトをオレンジに（ティール＆オレンジ）
/// - シャドウを紫に、ハイライトを黄金色に（ファンタジー風）
/// </summary>

/// トーン分離設定
struct TonalSplitSettings {
    // シャドウ側の色
    float shadowHue = 180.0f;         // 0-360: シャドウの色相
    float shadowSaturation = 0.0f;    // -100〜100: シャドウの彩度変更
    float shadowStrength = 0.0f;      // 0-1: シャドウへの適用強度

    // ハイライト側の色
    float highlightHue = 30.0f;       // 0-360: ハイライトの色相
    float highlightSaturation = 0.0f; // -100〜100: ハイライトの彩度変更
    float highlightStrength = 0.0f;   // 0-1: ハイライトへの適用強度

    // 分離点
    float splitPoint = 0.5f;          // 0-1: シャドウ/ハイライトの分離点
    float splitSoftness = 0.5f;       // 0-1: 分離の柔らかさ

    // マスター
    float masterStrength = 1.0f;      // 0-1: 全体の強度

    /// リセット
    void reset() {
        shadowHue = 180.0f;
        shadowSaturation = 0.0f;
        shadowStrength = 0.0f;
        highlightHue = 30.0f;
        highlightSaturation = 0.0f;
        highlightStrength = 0.0f;
        splitPoint = 0.5f;
        splitSoftness = 0.5f;
        masterStrength = 1.0f;
    }

    /// プリセット: ティール＆オレンジ（映画風）
    static TonalSplitSettings tealAndOrange() {
        TonalSplitSettings settings;
        settings.shadowHue = 180.0f;        // シアン
        settings.shadowSaturation = 30.0f;
        settings.shadowStrength = 0.4f;
        settings.highlightHue = 30.0f;      // オレンジ
        settings.highlightSaturation = 20.0f;
        settings.highlightStrength = 0.3f;
        settings.splitPoint = 0.45f;
        settings.splitSoftness = 0.6f;
        return settings;
    }

    /// プリセット: ファンタジー（紫＆黄金）
    static TonalSplitSettings fantasy() {
        TonalSplitSettings settings;
        settings.shadowHue = 280.0f;        // 紫
        settings.shadowSaturation = 25.0f;
        settings.shadowStrength = 0.35f;
        settings.highlightHue = 45.0f;      // 黄金
        settings.highlightSaturation = 30.0f;
        settings.highlightStrength = 0.3f;
        return settings;
    }

    /// プリセット: クール（青＆白）
    static TonalSplitSettings cool() {
        TonalSplitSettings settings;
        settings.shadowHue = 220.0f;        // 青
        settings.shadowSaturation = 20.0f;
        settings.shadowStrength = 0.3f;
        settings.highlightHue = 0.0f;       // 中立
        settings.highlightSaturation = -10.0f;
        settings.highlightStrength = 0.2f;
        return settings;
    }
};

/// トーン分離プロセッサ
class TonalSplitProcessor {
public:
    TonalSplitProcessor();
    ~TonalSplitProcessor();

    /// 設定
    void setSettings(const TonalSplitSettings& settings);
    auto settings() const -> const TonalSplitSettings&;

    /// 画像に適用
    auto apply(const QImage& source) const -> QImage;

    /// 単一ピクセルに適用（0-1範囲のRGB）
    void applyPixel(float& r, float& g, float& b) const;

private:
    TonalSplitSettings settings_;

    /// 分離マスクの取得
    auto getShadowMask(float luma) const -> float;
    auto getHighlightMask(float luma) const -> float;

    /// HSL変換ユーティリティ
    static void rgbToHsl(float r, float g, float b, float& h, float& s, float& l);
    static void hslToRgb(float h, float s, float l, float& r, float& g, float& b);

    /// 値のクランプ
    static auto clampValue(float value, float minVal, float maxVal) -> float;
};

} // namespace ArtifactCore
