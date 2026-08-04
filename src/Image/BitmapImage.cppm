module;
#include <algorithm>
#include <limits>
#include <utility>
#include <utility>

module Image.Bitmap;

namespace ArtifactCore {

class Bitmap::Impl {
public:
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixels;
};










 Bitmap::Bitmap()
    : impl_(new Impl()) {}

Bitmap::Bitmap(const int width, const int height)
    : impl_(new Impl()) {
  resize(width, height);
}

 Bitmap::~Bitmap()
 {
  delete impl_;
 }

Bitmap::Bitmap(Bitmap&& other) noexcept : impl_(std::exchange(other.impl_, nullptr)) {}

Bitmap& Bitmap::operator=(Bitmap&& other) noexcept {
  if (this == &other) return *this;
  delete impl_;
  impl_ = std::exchange(other.impl_, nullptr);
  return *this;
}

int Bitmap::width() const { return impl_ ? impl_->width : 0; }
int Bitmap::height() const { return impl_ ? impl_->height : 0; }
bool Bitmap::isValid() const {
  return impl_ && impl_->width > 0 && impl_->height > 0 &&
         impl_->pixels.size() == static_cast<std::size_t>(impl_->width) *
                                      static_cast<std::size_t>(impl_->height) * 4u;
}

void Bitmap::clear() {
  if (impl_) std::fill(impl_->pixels.begin(), impl_->pixels.end(), 0);
}

bool Bitmap::resize(const int width, const int height) {
  if (!impl_ || width < 0 || height < 0) return false;
  const auto w = static_cast<std::size_t>(width);
  const auto h = static_cast<std::size_t>(height);
  if (w != 0 && h > std::numeric_limits<std::size_t>::max() / w) return false;
  const auto pixels = w * h;
  if (pixels > std::numeric_limits<std::size_t>::max() / 4u) return false;
  impl_->width = width;
  impl_->height = height;
  impl_->pixels.assign(pixels * 4u, 0);
  return true;
}

bool Bitmap::setPixel(const int x, const int y, const std::uint8_t r,
                      const std::uint8_t g, const std::uint8_t b,
                      const std::uint8_t a) {
  if (!isValid() || x < 0 || y < 0 || x >= width() || y >= height()) return false;
  const auto offset = (static_cast<std::size_t>(y) * width() + x) * 4u;
  impl_->pixels[offset] = r;
  impl_->pixels[offset + 1] = g;
  impl_->pixels[offset + 2] = b;
  impl_->pixels[offset + 3] = a;
  return true;
}

bool Bitmap::pixel(const int x, const int y, std::uint8_t& r,
                   std::uint8_t& g, std::uint8_t& b, std::uint8_t& a) const {
  if (!isValid() || x < 0 || y < 0 || x >= width() || y >= height()) return false;
  const auto offset = (static_cast<std::size_t>(y) * width() + x) * 4u;
  r = impl_->pixels[offset];
  g = impl_->pixels[offset + 1];
  b = impl_->pixels[offset + 2];
  a = impl_->pixels[offset + 3];
  return true;
}

const std::vector<std::uint8_t>& Bitmap::data() const {
  static const std::vector<std::uint8_t> empty;
  return impl_ ? impl_->pixels : empty;
}

}
