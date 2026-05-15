module;
#include <QColor>
#include <QImage>

export module ImageProcessing.ColorTransform.Fill;

export namespace ArtifactCore {

struct SolidFillSettings {
    QColor color = QColor(255, 255, 255);
    float opacity = 1.0f;
    bool preserveAlpha = true;

    void reset() {
        *this = SolidFillSettings{};
    }

    static SolidFillSettings white();
    static SolidFillSettings black();
    static SolidFillSettings red();
    static SolidFillSettings blue();
    static SolidFillSettings green();
};

class SolidFillProcessor {
public:
    SolidFillProcessor();
    ~SolidFillProcessor();

    void setSettings(const SolidFillSettings& settings);
    const SolidFillSettings& settings() const;

    QImage apply(const QImage& source) const;
    void apply(float* pixels, int width, int height) const;

private:
    SolidFillSettings settings_;
};

} // namespace ArtifactCore
