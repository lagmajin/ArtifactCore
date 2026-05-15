module;
#include <algorithm>
#include <QColor>
#include <QImage>

export module ImageProcessing.ColorTransform.GradientRamp;

export namespace ArtifactCore {

struct GradientRampSettings {
    QColor startColor = QColor(0, 0, 0);
    QColor endColor = QColor(255, 255, 255);
    float startX = 0.0f;
    float startY = 0.0f;
    float endX = 1.0f;
    float endY = 1.0f;
    float opacity = 1.0f;
    bool preserveAlpha = true;

    void reset() {
        *this = GradientRampSettings{};
    }

    static GradientRampSettings sunrise();
    static GradientRampSettings ocean();
    static GradientRampSettings neon();
    static GradientRampSettings mono();
};

class GradientRampProcessor {
public:
    enum class Preset {
        Sunrise = 0,
        Ocean = 1,
        Neon = 2,
        Mono = 3,
        Custom = 4
    };

    GradientRampProcessor();
    ~GradientRampProcessor();

    void setSettings(const GradientRampSettings& settings);
    const GradientRampSettings& settings() const;
    void applyPreset(Preset preset);

    QImage apply(const QImage& source) const;
    void apply(float* pixels, int width, int height) const;

private:
    GradientRampSettings settings_;
};

} // namespace ArtifactCore
