module;
//import std;
#include <QImage>

#include <QtGui/QBitmap>

#include <opencv2/opencv.hpp>

#include "../Define/DllExportMacro.hpp"


export module Image.ImageYUV420;

import std;


export namespace ArtifactCore {

 class ImageF32x4_RGBA;

 class LIBRARY_DLL_API ImageYUV420 {
  private:
   class Impl;
   Impl* impl_;

   //std::vector<uint8_t> y_plane_;
   //std::vector<uint8_t> u_plane_;
   //std::vector<uint8_t> v_plane_;
 public:
	ImageYUV420();
    ImageYUV420(const QImage& image);
   ~ImageYUV420();
   static ImageYUV420 fromImage32xRGBA(const ImageF32x4_RGBA);

   static ImageYUV420 fromQImage(const QImage& image);
 };



}
