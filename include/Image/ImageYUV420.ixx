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
  public:
   ImageYUV420();
   ImageYUV420(const QImage& image);
   ~ImageYUV420();

   // Create from an ImageF32x4_RGBA (straight RGBA)
   static ImageYUV420 fromImage32xRGBA(const ImageF32x4_RGBA& rgba);

   // Convert back to ImageF32x4_RGBA
   ImageF32x4_RGBA toImage32xRGBA() const;

   // Create from raw Y/U/V planes (U/V may be subsampled H/2 x W/2)
   static ImageYUV420 fromPlanes(const cv::Mat& yPlane, const cv::Mat& uPlane, const cv::Mat& vPlane);

   // Access raw planes
   cv::Mat yPlane() const;
   cv::Mat uPlane() const;
   cv::Mat vPlane() const;
 };

}
