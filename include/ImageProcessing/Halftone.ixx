module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:Halftone;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

/// ドット形状
enum class HalftoneDotShape {
    Circle,
    Ellipse,
    Diamond,
    Line,
    Cross
};

/// カラーモード
enum class HalftoneColorMode {
    Monochrome,  ///< 単色（luminance → alpha）
    Color,       ///< RGB各チャンネル独立
    CMYK         ///< CMYK 4版（各版に角度推奨値）
};

struct HalftoneSettings {
    float dotSize = 8.0f;             ///< グリッドサイズ（ピクセル）
    float angle = 0.0f;               ///< グリッド回転（度）
    float contrast = 1.0f;            ///< コントラスト倍率
    HalftoneDotShape dotShape = HalftoneDotShape::Circle;
    HalftoneColorMode colorMode = HalftoneColorMode::Monochrome;

    /// CMYK 個別角度 (C, M, Y, K) — クラシックな推奨値は 15°, 75°, 0°, 45°
    float cmykAngles[4] = {15.0f, 75.0f, 0.0f, 45.0f};

    /// dotShape==Ellipse 時のアスペクト比（1.0 = 円）
    float ellipseAspect = 1.5f;
};

class LIBRARY_DLL_API Halftone {
public:
    Halftone() = default;
    ~Halftone() = default;

    void process(float4* buffer, int width, int height, const HalftoneSettings& settings);
    void process(ImageF32x4_RGBA& image, const HalftoneSettings& settings);
};

}
