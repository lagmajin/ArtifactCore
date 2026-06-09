module;
#include <algorithm>
#include <cmath>
export module ImageProcessing.ColorTransform.TriChromaticShift;

import Particle;
import FloatRGBA;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

/// トライクロマティック・シフトの3色セット
/// 各色は HSL で保持し、hueShift で一括回転する。
struct TriChromaticTriad {
    double shadowHue = 240.0;     ///< シャドウ色相（度）
    double shadowSat = 0.6;       ///< シャドウ彩度
    double shadowLum = 0.15;      ///< シャドウ輝度（出力の明るさ）

    double midtoneHue = 0.0;      ///< ミッドトーン色相
    double midtoneSat = 0.05;     ///< ミッドトーン彩度
    double midtoneLum = 0.5;

    double highlightHue = 45.0;   ///< ハイライト色相
    double highlightSat = 0.7;    ///< ハイライト彩度
    double highlightLum = 0.9;

    /// プリセット: cinematic blue/orange
    static TriChromaticTriad cinematic();
    /// プリセット: teal & orange (blockbuster)
    static TriChromaticTriad tealAndOrange();
    /// プリセット: purple/green/gold (Joker-style)
    static TriChromaticTriad joker();
    /// プリセット: cold blue/magenta (cyberpunk)
    static TriChromaticTriad cyberpunk();
};

/// トライクロマティック・シフト設定
struct TriChromaticSettings {
    TriChromaticTriad triad;       ///< ベース3色

    double hueShift = 0.0;        ///< 全色の色相を一括シフト（度）← 1スライダーで激変
    double balance = 0.5;         ///< シャドウ↔ハイライトの遷移バランス
    double softness = 0.55;       ///< 遷移のソフトネス
    double colorMix = 0.85;       ///< 彩色の強さ（0=入力そのまま）
    double masterStrength = 1.0;  ///< 全体の強さ
    bool preserveLuma = true;     ///< 輝度を保存
};

/// トライクロマティック・シフト処理
class TriChromaticProcessor {
public:
    TriChromaticProcessor();
    ~TriChromaticProcessor();

    void setSettings(const TriChromaticSettings& settings);
    const TriChromaticSettings& settings() const;

    /// ImageF32x4_RGBA に適用（インプレース）
    void apply(ImageF32x4_RGBA& image) const;

    /// float4 生バッファに適用
    void apply(float4* buffer, int width, int height) const;

    /// 単一ピクセルに適用
    void applyPixel(float& r, float& g, float& b) const;

private:
    TriChromaticSettings settings_;

    static double luma(double r, double g, double b);
    static void hslToRgb(double h, double s, double l, double& r, double& g, double& b);
    static void rgbToHsl(double r, double g, double b, double& h, double& s, double& l);
};

}
