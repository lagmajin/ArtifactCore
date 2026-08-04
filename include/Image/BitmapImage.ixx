module;
#include <utility>
#include <cstdint>
#include <vector>
// import <cstdint>;
//#include <QtGui/QImage>

//#include <QtGui/QBitmap>

#include "../Define/DllExportMacro.hpp"

export module Image.Bitmap;

export namespace ArtifactCore {
 
 

 class LIBRARY_DLL_API Bitmap {
 private:
  class Impl;
  Impl* impl_ = nullptr;
 public:
  Bitmap();
  Bitmap(int width, int height);
  ~Bitmap();

  Bitmap(const Bitmap&) = delete;
  Bitmap& operator=(const Bitmap&) = delete;
  Bitmap(Bitmap&&) noexcept;
  Bitmap& operator=(Bitmap&&) noexcept;

  int width() const;
  int height() const;
  bool isValid() const;
  void clear();
  bool resize(int width, int height);
  bool setPixel(int x, int y, std::uint8_t r, std::uint8_t g,
                std::uint8_t b, std::uint8_t a = 255);
  bool pixel(int x, int y, std::uint8_t& r, std::uint8_t& g,
             std::uint8_t& b, std::uint8_t& a) const;
  const std::vector<std::uint8_t>& data() const;
 };







}
