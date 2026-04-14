module;

#include <algorithm>
#include <cmath>
#include <vector>
#include <QString>
#include <QImage>

export module ImageProcessing.ColorTransform.HueSaturation;

export namespace ArtifactCore {

// ============================================================================
// Hue/Saturation (色相/彩度調整)
// ============================================================================

/// 色相/彩度設定
struct HueSaturationSettings {
    // マスター調整
    double masterHue = 0.0;        ///< 全体色相回転 (-180 to 180)
    double masterSaturation = 0.0;  ///< 全体彩度調整 (-100 to 100)
    double masterLightness = 0.0;   ///< 全体明度調整 (-100 to 100)
    
    // チャンネル別調整
    double redHue = 0.0;           ///< 赤チャンネル色相
    double redSaturation = 0.0;     ///< 赤チャンネル彩度
    double redLightness = 0.0;      ///< 赤チャンネル明度
    
    double yellowHue = 0.0;         ///< 黄チャンネル色相
    double yellowSaturation = 0.0;  ///< 黄チャンネル彩度
    double yellowLightness = 0.0;   ///< 黄チャンネル明度
    
    double greenHue = 0.0;          ///< 緑チャンネル色相
    double greenSaturation = 0.0;   ///< 緑チャンネル彩度
    double greenLightness = 0.0;    ///< 緑チャンネル明度
    
    double cyanHue = 0.0;           ///< シアンチャンネル色相
    double cyanSaturation = 0.0;    ///< シアンチャンネル彩度
    double cyanLightness = 0.0;     ///< シアンチャンネル明度
    
    double blueHue = 0.0;           ///< 青チャンネル色相
    double blueSaturation = 0.0;    ///< 青チャンネル彩度
    double blueLightness = 0.0;     ///< 青チャンネル明度
    
    double magentaHue = 0.0;        ///< マゼンタチャンネル色相
    double magentaSaturation = 0.0; ///< マゼンタチャンネル彩度
    double magentaLightness = 0.0;  ///< マゼンタチャンネル明度
    
    /// リセット
    void reset() {
        masterHue = 0.0;
        masterSaturation = 0.0;
        masterLightness = 0.0;
        redHue = redSaturation = redLightness = 0.0;
        yellowHue = yellowSaturation = yellowLightness = 0.0;
        greenHue = greenSaturation = greenLightness = 0.0;
        cyanHue = cyanSaturation = cyanLightness = 0.0;
        blueHue = blueSaturation = blueLightness = 0.0;
        magentaHue = magentaSaturation = magentaLightness = 0.0;
    }
    
    /// プリセット
    static HueSaturationSettings normal() { return HueSaturationSettings{}; }
    static HueSaturationSettings increaseSaturation();
    static HueSaturationSettings decreaseSaturation();
    static HueSaturationSettings colorize(double hue, double saturation);
};

/// 色相/彩度エフェクト
class HueSaturationEffect {
public:
    HueSaturationEffect();
    ~HueSaturationEffect();
    
    /// 設定
    void setSettings(const HueSaturationSettings& settings);
    HueSaturationSettings settings() const;
    
    /// 画像に適用
    QImage apply(const QImage& source) const;
    
    /// 単一ピクセルに適用（0-1範囲）
    void applyPixel(float& r, float& g, float& b) const;

private:
    class Impl;
    Impl* impl_;
    
    /// HSL変換ユーティリティ
    static void rgbToHsl(float r, float g, float b, float& h, float& s, float& l);
    static void hslToRgb(float h, float s, float l, float& r, float& g, float& b);
    
    /// 色相調整
    static float adjustHue(float hue, float adjustment);
    
    /// 彩度/明度調整
    static float adjustSaturation(float saturation, float adjustment);
    static float adjustLightness(float lightness, float adjustment);
    
    /// チャンネル別調整
    void applyChannelAdjustment(float& h, float& s, float& l) const;
};

} // namespace ArtifactCore