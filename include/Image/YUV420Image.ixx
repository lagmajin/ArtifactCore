module;
#include <stdint.h>
#include <QtGui/QImage>

#include <QtGui/QBitmap>

#include <opencv2/opencv.hpp>

#include "../Define/DllExportMacro.hpp"

export module YUV20Image;



namespace ArtifactCore {


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
 };



}
