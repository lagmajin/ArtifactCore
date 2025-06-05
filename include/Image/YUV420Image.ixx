module;
//import std;
#include <QtGui/QImage>

#include <QtGui/QBitmap>

#include <opencv2/opencv.hpp>

#include "../Define/DllExportMacro.hpp"

export module YUV20Image;

import ImageF32x4_RGBA;


export namespace ArtifactCore {


 export class YUV420Image {
  private:
   int width_;
   int height_;
   std::vector<uint8_t> y_plane_;
   std::vector<uint8_t> u_plane_;
   std::vector<uint8_t> v_plane_;
 public:
	YUV420Image();
   ~YUV420Image();
   void fromImage32xRGBA(const ImageF32x4_RGBA);
 };



}
