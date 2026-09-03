module;
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
  bool isEmpty() const noexcept { return width_ <= 0 || height_ <= 0 || values_.isEmpty(); }
  int width() const noexcept { return width_; }
  int height() const noexcept { return height_; }
  float value(int x, int y) const noexcept { return values_[y * width_ + x]; }
private:
  int width_ = 0, height_ = 0;
  QVector<float> values_;
};
}
