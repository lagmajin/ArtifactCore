module;
#include <algorithm>
#include <cmath>
#include <QImage>

export module ImageProcessing.ColorTransform.PhotoFilter;

export namespace ArtifactCore {

struct PhotoFilterColor {
    float r = 1.0f;
    float g = 0.9f;
    float b = 0.7f;
};

enum class PhotoFilterPreset {
    Warm = 0,
    Cool = 1,
    Sepia = 2,
    Cyan = 3,
    Rose = 4
};

struct PhotoFilterSettings {
    PhotoFilterColor color {1.0f, 0.9f, 0.7f};
    float density = 0.35f;
    float brightness = 0.0f;
    float contrast = 1.0f;
    float saturationBoost = 1.0f;
    bool preserveLuma = true;

    void reset() {
        *this = PhotoFilterSettings{};
    }

    static PhotoFilterSettings warm();
    static PhotoFilterSettings cool();
    static PhotoFilterSettings sepia();
    static PhotoFilterSettings cyan();
    static PhotoFilterSettings rose();
};

class PhotoFilterProcessor {
public:
    PhotoFilterProcessor();
    ~PhotoFilterProcessor();

    void setSettings(const PhotoFilterSettings& settings);
    const PhotoFilterSettings& settings() const;

    QImage apply(const QImage& source) const;
    void applyPixel(float& r, float& g, float& b) const;

private:
    PhotoFilterSettings settings_;

    static float luma(float r, float g, float b);
    static void rgbToHsl(float r, float g, float b, float& h, float& s, float& l);
    static void hslToRgb(float h, float s, float l, float& r, float& g, float& b);
    static void preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled);
};

} // namespace ArtifactCore
