module;
#include <algorithm>
#include <cstdint>
#include <vector>
#include <QImage>
#include <QString>

export module Image.DepthMap;

export namespace ArtifactCore {

class DepthMap {
public:
    DepthMap() = default;
    DepthMap(int width, int height, float value = 0.0f);

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    bool isEmpty() const noexcept { return width_ <= 0 || height_ <= 0; }

    float at(int x, int y) const noexcept;
    float sampleBilinear(float u, float v) const noexcept;
    void set(int x, int y, float value) noexcept;
    void fill(float value) noexcept;
    void normalize();
    void invert() noexcept;

    // QImage is an explicit asset/input boundary; the internal representation
    // remains a clamped float field.
    static DepthMap fromQImage(const QImage& image);
    QImage toQImage() const;
    bool load(const QString& path);
    bool save(const QString& path) const;

    const std::vector<float>& values() const noexcept { return values_; }
    std::vector<float>& values() noexcept { return values_; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<float> values_;
};

}
