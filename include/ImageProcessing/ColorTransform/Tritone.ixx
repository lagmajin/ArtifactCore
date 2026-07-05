module;
#include <algorithm>
#include <cmath>
#include <QImage>

export module ImageProcessing.ColorTransform.Tritone;

export namespace ArtifactCore {

struct TritoneColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

struct TritoneSettings {
    TritoneColor shadowColor {0.12f, 0.17f, 0.32f};
    TritoneColor midtoneColor {0.50f, 0.50f, 0.50f};
    TritoneColor highlightColor {0.95f, 0.82f, 0.50f};

    float balance = 0.5f;
    float softness = 0.55f;
    float shadowStrength = 1.0f;
    float midtoneStrength = 1.0f;
    float highlightStrength = 1.0f;
    float masterStrength = 1.0f;
    float colorMix = 0.85f;
    bool preserveLuma = true;

    void reset() {
        *this = TritoneSettings{};
    }

    static TritoneSettings cinematic();
    static TritoneSettings tealAndOrange();
};

class TritoneProcessor {
public:
    TritoneProcessor();
    ~TritoneProcessor();

    void setSettings(const TritoneSettings& settings);
    const TritoneSettings& settings() const;

    QImage apply(const QImage& source) const;
    void applyPixel(float& r, float& g, float& b) const;

private:
    TritoneSettings settings_;

    static float luma(float r, float g, float b);
    static float smoothstep(float edge0, float edge1, float x);
    static void preserveLuma(float sourceLuma, float& r, float& g, float& b, float preserveAmount);
};

} // namespace ArtifactCore
