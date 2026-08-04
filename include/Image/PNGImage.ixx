module;
#include <utility>
#include <QByteArray>

export module Image.Png;

namespace ArtifactCore {

 class PNGImage
 {
 public:
  PNGImage() = default;
  bool loadHeader(const QByteArray& encoded);
  bool isValid() const { return valid_; }
  int width() const { return width_; }
  int height() const { return height_; }
  int bitDepth() const { return bitDepth_; }
  int colorType() const { return colorType_; }
  void clear();

 private:
  int width_ = 0;
  int height_ = 0;
  int bitDepth_ = 0;
  int colorType_ = 0;
  bool valid_ = false;
 };



};
