module;
#include <cstdint>
#include <climits>
#include <utility>

module Image.Png;

namespace ArtifactCore{

namespace {
std::uint32_t readBE32(const char* bytes) {
  const auto* p = reinterpret_cast<const std::uint8_t*>(bytes);
  return (static_cast<std::uint32_t>(p[0]) << 24) |
         (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) |
         static_cast<std::uint32_t>(p[3]);
}
}

bool PNGImage::loadHeader(const QByteArray& encoded) {
  clear();
  constexpr char signature[] = "\x89PNG\x0D\x0A\x1A\x0A";
  if (encoded.size() < 33 || encoded.left(8) != QByteArray(signature, 8))
    return false;
  const char* ihdr = encoded.constData() + 8;
  if (readBE32(ihdr) != 13 || QByteArray(ihdr + 4, 4) != QByteArrayLiteral("IHDR"))
    return false;
  const auto width = readBE32(ihdr + 8);
  const auto height = readBE32(ihdr + 12);
  if (width == 0 || height == 0 || width > static_cast<std::uint32_t>(INT_MAX) ||
      height > static_cast<std::uint32_t>(INT_MAX)) return false;
  width_ = static_cast<int>(width);
  height_ = static_cast<int>(height);
  bitDepth_ = static_cast<unsigned char>(ihdr[16]);
  colorType_ = static_cast<unsigned char>(ihdr[17]);
  valid_ = bitDepth_ > 0;
  return valid_;
}

void PNGImage::clear() {
  width_ = height_ = bitDepth_ = colorType_ = 0;
  valid_ = false;
}







}
