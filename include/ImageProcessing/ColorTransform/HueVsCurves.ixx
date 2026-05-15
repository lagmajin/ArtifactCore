module;
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>
#include <QString>
#include <QImage>

export module ImageProcessing.ColorTransform.HueVsCurves;

export namespace ArtifactCore {

/// <summary>
/// Hue vs Hue / Hue vs Sat / Hue vs Luma カーブ制御
/// 
/// 特定色相範囲（例: 赤 0-30°、肌色 15-45°など）に対して
/// 色相/彩度/明度をカーブで制御する。
/// DaVinci ResolveのHue vs Hue/Sat/Lum相当。
/// </summary>

/// Hue vs X カーブの制御点
struct HueControlPoint {
    float hue = 0.0f;           // 色相 (0-360°)
    float value = 0.0f;         // 補正値
                                //   Hue vs Hue: -180〜180°
                                //   Hue vs Sat: -100〜100%
                                //   Hue vs Luma: -100〜100%
    float range = 30.0f;        // 影響範囲 (°)
    bool enabled = true;
};

/// Hue vs カーブの種類
enum class HueCurveType {
    HueVsHue,       // 色相 vs 色相
    HueVsSat,       // 色相 vs 彩度
    HueVsLuma       // 色相 vs 明度
};

/// Hue vs カーブ設定
struct HueVsCurveSettings {
    HueCurveType curveType = HueCurveType::HueVsHue;
    std::vector<HueControlPoint> controlPoints;
    
    // 全体の強度
    float masterStrength = 1.0f;  // 0-1
    
    /// リセット
    void reset() {
        controlPoints.clear();
        masterStrength = 1.0f;
    }
    
    /// 制御点追加
    void addControlPoint(float hue, float value, float range = 30.0f) {
        controlPoints.push_back({hue, value, range, true});
    }
    
    /// プリセット: 肌色補正（Hue vs Sat）
    static HueVsCurveSettings skinToneSaturationBoost() {
        HueVsCurveSettings settings;
        settings.curveType = HueCurveType::HueVsSat;
        settings.addControlPoint(15.0f, 20.0f, 30.0f);  // 肌色領域の彩度+20
        settings.addControlPoint(30.0f, 15.0f, 20.0f);
        return settings;
    }
    
    /// プリセット: 空の色相シフト（Hue vs Hue）
    static HueVsCurveSettings skyHueShift() {
        HueVsCurveSettings settings;
        settings.curveType = HueCurveType::HueVsHue;
        settings.addControlPoint(200.0f, -15.0f, 40.0f);  // 青→シアン寄りに
        settings.addControlPoint(220.0f, -20.0f, 30.0f);
        return settings;
    }
    
    /// プリセット: シャドウを暖色に（Hue vs Luma）
    static HueVsCurveSettings warmShadows() {
        HueVsCurveSettings settings;
        settings.curveType = HueCurveType::HueVsLuma;
        settings.addControlPoint(0.0f, 10.0f, 60.0f);   // 暗部を明るく
        settings.addControlPoint(30.0f, 5.0f, 40.0f);
        return settings;
    }
};

/// Hue vs カーブプロセッサ
class HueVsCurveProcessor {
public:
    HueVsCurveProcessor();
    ~HueVsCurveProcessor();

    /// 設定
    void setSettings(const HueVsCurveSettings& settings);
    auto settings() const -> const HueVsCurveSettings&;

    /// 画像に適用
    auto apply(const QImage& source) const -> QImage;

    /// 単一ピクセルに適用（0-1範囲のRGB）
    void applyPixel(float& r, float& g, float& b) const;

private:
    HueVsCurveSettings settings_;

    /// カーブ値の取得（線形補間）
    auto getCurveValue(float hue) const -> float;

    /// 色相の範囲内か判定（重み付き）
    auto getHueWeight(float pixelHue, const HueControlPoint& point) const -> float;

    /// HSL変換ユーティリティ
    static void rgbToHsl(float r, float g, float b, float& h, float& s, float& l);
    static void hslToRgb(float h, float s, float l, float& r, float& g, float& b);

    /// 値のクランプ
    static auto clampValue(float value, float minVal, float maxVal) -> float;
};

} // namespace ArtifactCore
