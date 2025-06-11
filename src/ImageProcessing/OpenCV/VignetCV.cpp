module;
#include "../../../include/Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>


module ImageProcessing:Vignette;




namespace ArtifactCore {

 LIBRARY_DLL_API cv::Mat vignetteEffect(const cv::Mat& input) {
  cv::Mat mask(input.size(), CV_32F);
  float cx = input.cols / 2.0f;
  float cy = input.rows / 2.0f;
  float maxDist = std::sqrt(cx * cx + cy * cy);
  for (int y = 0; y < input.rows; ++y) {
   for (int x = 0; x < input.cols; ++x) {
	float dx = x - cx;
	float dy = y - cy;
	float dist = std::sqrt(dx * dx + dy * dy);
	mask.at<float>(y, x) = 1.0f - (dist / maxDist) * 0.6f;
   }
  }
  std::vector<cv::Mat> channels;
  input.convertTo(input, CV_32F, 1.0 / 255.0);
  cv::split(input, channels);
  for (auto& ch : channels)
   ch = ch.mul(mask);
  cv::Mat merged;
  cv::merge(channels, merged);
  merged.convertTo(merged, CV_8UC3, 255.0);
  return merged;
 }


}