module;
//import std;
#include <QtGui/QImage>

#include <QtGui/QBitmap>

#include <opencv2/opencv.hpp>

#include "../Define/DllExportMacro.hpp"


export module Image:ImageYUV420;




export namespace ArtifactCore {

 class ImageF32x4_RGBA;

 export class ImageYUV420 {
  private:
   int width_;
   int height_;
   std::vector<uint8_t> y_plane_;
   std::vector<uint8_t> u_plane_;
   std::vector<uint8_t> v_plane_;
 public:
	ImageYUV420();
   ~ImageYUV420();
   void fromImage32xRGBA(const ImageF32x4_RGBA);
 };



}
