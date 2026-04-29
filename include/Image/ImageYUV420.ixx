module;
//
#include "../Define/DllExportMacro.hpp"


#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QImage>
#include <QtGui/QBitmap>
export module Image.ImageYUV420;

namespace cv {
  class Mat;
}






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
