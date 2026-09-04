module;
#include <algorithm>
#include <cmath>
#include <QImage>
#include <QVector>
#include <QString>

export module Image.DepthMap;

export namespace ArtifactCore {
class DepthMap {
public:
  bool load(const QString& path) {
    const QImage image(path);
    if (image.isNull()) { clear(); return false; }
    width_ = image.width(); height_ = image.height();
    values_.resize(width_ * height_);
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    for (int y = 0; y < height_; ++y)
      for (int x = 0; x < width_; ++x)
        values_[y * width_ + x] = gray.constScanLine(y)[x] / 255.0f;
    return true;
  }
  void clear() { width_ = height_ = 0; values_.clear(); }
  void resize(int width, int height, float value = 0.0f) {
    if (width <= 0 || height <= 0) { clear(); return; }
    width_ = width;
    height_ = height;
    values_.fill(std::clamp(value, 0.0f, 1.0f), width_ * height_);
  }
  bool isEmpty() const noexcept { return width_ <= 0 || height_ <= 0 || values_.isEmpty(); }
  int width() const noexcept { return width_; }
  int height() const noexcept { return height_; }
  float value(int x, int y) const noexcept { return values_[y * width_ + x]; }
  void setValue(int x, int y, float value) noexcept {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) { return; }
    values_[y * width_ + x] = std::clamp(value, 0.0f, 1.0f);
  }
  float sampleBilinear(float normalizedX, float normalizedY) const noexcept {
    if (isEmpty()) { return 0.0f; }
    const float x = std::clamp(normalizedX, 0.0f, 1.0f) * static_cast<float>(width_ - 1);
    const float y = std::clamp(normalizedY, 0.0f, 1.0f) * static_cast<float>(height_ - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, width_ - 1);
    const int y1 = std::min(y0 + 1, height_ - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const float top = value(x0, y0) + (value(x1, y0) - value(x0, y0)) * tx;
    const float bottom = value(x0, y1) + (value(x1, y1) - value(x0, y1)) * tx;
    return top + (bottom - top) * ty;
  }
private:
  int width_ = 0, height_ = 0;
  QVector<float> values_;
};
}
