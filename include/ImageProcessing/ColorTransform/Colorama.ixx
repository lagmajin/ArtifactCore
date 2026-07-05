module;
#include <algorithm>
#include <array>
#include <cmath>
#include <QImage>

export module ImageProcessing.ColorTransform.Colorama;

export namespace ArtifactCore {

struct ColoramaColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

enum class ColoramaSourceMode {
    Luma = 0,
    Hue = 1
};

enum class ColoramaPalette {
    Rainbow = 0,
    Fire = 1,
    Ocean = 2,
    Neon = 3,
    Sunset = 4
};

struct ColoramaSettings {
    ColoramaSourceMode sourceMode = ColoramaSourceMode::Luma;
    ColoramaPalette palette = ColoramaPalette::Rainbow;
    float phase = 0.0f;
    float spread = 1.0f;
    float strength = 1.0f;
    float saturationBoost = 1.0f;
    float contrast = 1.0f;
    bool preserveLuma = true;

    void reset() {
        *this = ColoramaSettings{};
    }

    static ColoramaSettings rainbow();
    static ColoramaSettings fire();
    static ColoramaSettings ocean();
};

class ColoramaProcessor {
public:
    ColoramaProcessor();
    ~ColoramaProcessor();

    void setSettings(const ColoramaSettings& settings);
    const ColoramaSettings& settings() const;

    QImage apply(const QImage& source) const;
    void applyPixel(float& r, float& g, float& b) const;

private:
    ColoramaSettings settings_;

    static float luma(float r, float g, float b);
    static void rgbToHsl(float r, float g, float b, float& h, float& s, float& l);
    static void hslToRgb(float h, float s, float l, float& r, float& g, float& b);
    static ColoramaColor samplePalette(ColoramaPalette palette, float t);
    static ColoramaColor interpolate(const ColoramaColor& a, const ColoramaColor& b, float t);
    static void preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled);
};

} // namespace ArtifactCore
