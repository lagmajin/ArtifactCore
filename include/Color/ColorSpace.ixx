module;

#include "../Define/DllExportMacro.hpp"

export module Color.ColorSpace;

import std;

export namespace ArtifactCore
{

// カラースペース
enum class ColorSpace
{
    Linear,         // リニア
    sRGB,           // sRGB
    Rec709,         // Rec.709 / Rec.1886
    Rec2020,       // Rec.2020 / Rec.2100 PQ
    P3,             // DCI-P3
    ACES_AP0,       // ACEScg
    ACES_AP1,       // ACEScct
};

// ガンマ関数
enum class GammaFunction
{
    Linear,
    sRGB,
    Gamma22,
    Gamma24,
    Gamma26,
    PQ,             // Perceptual Quantizer (HDR)
    HLG,            // Hybrid Log-Gamma (HDR)
};

// HDRダイナミックレンジ
enum class HDRMode
{
    SDR,            // Standard Dynamic Range
    HDR10,          // 1000 nits max
    HDR10Plus,      // 4000 nits max
    HLG,            // Hybrid Log-Gamma
    DolbyVision,    // Dolby Vision
};

// カラースペース変換
class LIBRARY_DLL_API ColorSpaceConverter
{
public:
    ColorSpaceConverter() = default;
    ~ColorSpaceConverter() = default;

    // 変換行列取得
    static std::array<float, 16> getConversionMatrix(ColorSpace from, ColorSpace to);

    // ガンマ適用
    static float applyGamma(float value, GammaFunction gamma);
    static float removeGamma(float value, GammaFunction gamma);

    // カラースペース情報
    static float getWhitePointX(ColorSpace space);
    static float getWhitePointY(ColorSpace space);
    static float getGammaExponent(ColorSpace space);
};

// HDRメタデータ
struct HDRMetadata
{
    float maxContentLightLevel = 1000.0f;      // nits
    float maxFrameAverageLightLevel = 500.0f;  // nits
    float averageBrightness = 200.0f;          // nits
    float minLuminance = 0.001f;               // nits
    int maxCLL = 1000;                         // Content Light Level
    int maxFALL = 500;                         // Frame Average Light Level

    // SMPTE ST 2086 メタデータ
    int displayPrimariesX[3] = {0};
    int displayPrimariesY[3] = {0};
    int whitePointX = 15635;   // 0.15635 in 0.00002 units
    int whitePointY = 16450;   // 0.16450
    int maxLuminance = 1000000; // 1000 nits
    int minLuminance = 1;       // 0.0001 nits
};

// カラーマネージャー
class LIBRARY_DLL_API ColorManager
{
public:
    static ColorManager& instance();

    ColorManager();
    ~ColorManager();

    // 作業カラースペース
    void setWorkingSpace(ColorSpace space);
    ColorSpace getWorkingSpace() const;

    // 入力カラースペース
    void setSourceSpace(ColorSpace space);
    ColorSpace getSourceSpace() const;

    // 出力カラースペース
    void setOutputSpace(ColorSpace space);
    ColorSpace getOutputSpace() const;

    // ガンマ設定
    void setGammaFunction(GammaFunction gamma);
    GammaFunction getGammaFunction() const;

    // HDR設定
    void setHDRMode(HDRMode mode);
    HDRMode getHDRMode() const;

    void setHDRMetadata(const HDRMetadata& metadata);
    HDRMetadata getHDRMetadata() const;

    void setMaxNits(float nits);
    float getMaxNits() const;

    // ビット深度
    void setBitDepth(int bits);
    int getBitDepth() const;

    // 変換
    std::array<float, 16> getConversionToLinear(ColorSpace space) const;
    std::array<float, 16> getConversionFromLinear(ColorSpace space) const;

Q_SIGNALS:
    void colorSpaceChanged(ColorSpace space);
    void hdrModeChanged(HDRMode mode);

private:
    ColorManager(const ColorManager&) = delete;
    ColorManager& operator=(const ColorManager&) = delete;

    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
