module;
#include <algorithm>
#include <cmath>
#include <limits>

module Image.DepthMap;

namespace ArtifactCore {

DepthMap::DepthMap(int width, int height, float value)
    : width_(std::max(0, width)), height_(std::max(0, height)),
      values_(static_cast<size_t>(width_) * static_cast<size_t>(height_),
              std::clamp(value, 0.0f, 1.0f)) {}

float DepthMap::at(int x, int y) const noexcept
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return 0.0f;
    }
    return values_[static_cast<size_t>(y) * static_cast<size_t>(width_) +
                   static_cast<size_t>(x)];
}

float DepthMap::sampleBilinear(float u, float v) const noexcept
{
    if (isEmpty()) return 0.0f;
    const float px = std::clamp(u, 0.0f, 1.0f) * static_cast<float>(width_ - 1);
    const float py = std::clamp(v, 0.0f, 1.0f) * static_cast<float>(height_ - 1);
    const int x0 = static_cast<int>(std::floor(px));
    const int y0 = static_cast<int>(std::floor(py));
    const int x1 = std::min(width_ - 1, x0 + 1);
    const int y1 = std::min(height_ - 1, y0 + 1);
    const float tx = px - static_cast<float>(x0);
    const float ty = py - static_cast<float>(y0);
    const float top = at(x0, y0) * (1.0f - tx) + at(x1, y0) * tx;
    const float bottom = at(x0, y1) * (1.0f - tx) + at(x1, y1) * tx;
    return top * (1.0f - ty) + bottom * ty;
}

void DepthMap::set(int x, int y, float value) noexcept
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    values_[static_cast<size_t>(y) * static_cast<size_t>(width_) +
            static_cast<size_t>(x)] = std::clamp(value, 0.0f, 1.0f);
}

void DepthMap::fill(float value) noexcept
{
    std::fill(values_.begin(), values_.end(), std::clamp(value, 0.0f, 1.0f));
}

void DepthMap::normalize()
{
    if (values_.empty()) return;
    const auto [minIt, maxIt] = std::minmax_element(values_.begin(), values_.end());
    const float range = *maxIt - *minIt;
    if (range <= std::numeric_limits<float>::epsilon()) {
        fill(0.0f);
        return;
    }
    for (float& value : values_) {
        value = std::clamp((value - *minIt) / range, 0.0f, 1.0f);
    }
}

void DepthMap::invert() noexcept
{
    for (float& value : values_) value = 1.0f - std::clamp(value, 0.0f, 1.0f);
}

DepthMap DepthMap::fromQImage(const QImage& image)
{
    if (image.isNull()) return {};
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    DepthMap result(gray.width(), gray.height());
    for (int y = 0; y < gray.height(); ++y) {
        const auto* row = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            result.set(x, y, static_cast<float>(row[x]) / 255.0f);
        }
    }
    return result;
}

QImage DepthMap::toQImage() const
{
    if (isEmpty()) return {};
    QImage image(width_, height_, QImage::Format_Grayscale8);
    for (int y = 0; y < height_; ++y) {
        auto* row = image.scanLine(y);
        for (int x = 0; x < width_; ++x) {
            const float value = std::clamp(at(x, y), 0.0f, 1.0f);
            row[x] = static_cast<uchar>(std::lround(value * 255.0f));
        }
    }
    return image;
}

bool DepthMap::load(const QString& path)
{
    const QImage image(path);
    if (image.isNull()) return false;
    *this = fromQImage(image);
    return !isEmpty();
}

bool DepthMap::save(const QString& path) const
{
    return !isEmpty() && toQImage().save(path);
}

}
