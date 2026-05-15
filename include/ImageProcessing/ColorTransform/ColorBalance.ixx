module;
#include <algorithm>
#include <cmath>
#include <QImage>

export module ImageProcessing.ColorTransform.ColorBalance;

export namespace ArtifactCore {

struct ColorBalanceSettings {
    float shadowR = 0.0f;
    float shadowG = 0.0f;
    float shadowB = 0.0f;
    float midtoneR = 0.0f;
    float midtoneG = 0.0f;
    float midtoneB = 0.0f;
    float highlightR = 0.0f;
    float highlightG = 0.0f;
    float highlightB = 0.0f;
    float shadowRange = 0.33f;
    float highlightRange = 0.66f;
    float masterStrength = 1.0f;
    bool preserveLuma = false;

    void reset() {
        *this = ColorBalanceSettings{};
    }

    static ColorBalanceSettings neutral();
    static ColorBalanceSettings coolShadows();
    static ColorBalanceSettings warmHighlights();
    static ColorBalanceSettings cinematic();
};

class ColorBalanceProcessor {
public:
    ColorBalanceProcessor();
    ~ColorBalanceProcessor();

    void setSettings(const ColorBalanceSettings& settings);
    const ColorBalanceSettings& settings() const;

    QImage apply(const QImage& source) const;
    void applyPixel(float& r, float& g, float& b) const;

private:
    ColorBalanceSettings settings_;

    static float luma(float r, float g, float b);
    static float smoothstep(float edge0, float edge1, float x);
    static void preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled, float strength);
};

} // namespace ArtifactCore
