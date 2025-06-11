
module;
#include <opencv2/opencv.hpp>
export module BloomCV;




namespace ArtifactCore {

 void BloomCV(const cv::Mat& src, float threshold = 200, int blurSize = 15, float bloomIntensity = 1.0f) 
 {
  cv::Mat gray;
  if (src.channels() == 3) {
   cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
  }
  else {
   gray = src;
  }


 };

}