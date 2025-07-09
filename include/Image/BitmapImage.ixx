module;
// import <cstdint>;
//#include <QtGui/QImage>

//#include <QtGui/QBitmap>

#include "../Define/DllExportMacro.hpp"

export module Image.Bitmap;

namespace ArtifactCore {
 
 class BitmapPrivate;

 class LIBRARY_DLL_API Bitmap {
 private:
  class Impl;
  Impl* impl;
 public:
  Bitmap();
  ~Bitmap();
 };







}