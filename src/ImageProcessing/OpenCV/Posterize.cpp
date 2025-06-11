module;

#include <opencv2/opencv.hpp>
#include "../../../include/Define/DllExportMacro.hpp"

module ImageProcessing:Posterize;



namespace ArtifactCore {

 LIBRARY_DLL_API cv::Mat posterizeEffect(const cv::Mat& input, int levels) {
  cv::Mat result = input.clone();
  int div = 256 / levels;
  result = result / div * div + div / 2;
  return result;
 }


}