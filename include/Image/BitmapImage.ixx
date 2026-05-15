module;
#include <utility>
// import <cstdint>;
//#include <QtGui/QImage>

//#include <QtGui/QBitmap>

#include "../Define/DllExportMacro.hpp"

export module Image.Bitmap;

export namespace ArtifactCore {
 
 

 class LIBRARY_DLL_API Bitmap {
 private:
  class Impl;
  Impl* impl;
 public:
  Bitmap();
  ~Bitmap();
 };







}
