module;
#include <stdint.h>
#include <QtGui/QImage>

#include <QtGui/QBitmap>

#include "../Define/DllExportMacro.hpp"

export module Image;

namespace ArtifactCore {
 
 class BitmapPrivate;

 class LIBRARY_DLL_API Bitmap {
 private:

 public:
  Bitmap();
  ~Bitmap();
 };







}