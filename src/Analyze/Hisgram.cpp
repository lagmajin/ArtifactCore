module;

#include <opencv2/opencv.hpp>
module Analyze:Histgram;

import std;

namespace ArtifactCore {

 std::vector<cv::Mat> computeHistogramRGBA32F(const cv::Mat& image, int histSize = 256, float rangeMin = 0.0f, float rangeMax = 1.0f) {
  CV_Assert(image.type() == CV_32FC4);

  std::vector<cv::Mat> channels(4);
  cv::split(image, channels);

  std::vector<cv::Mat> histograms(4);
  const float range[] = { rangeMin, rangeMax };
  const float* histRange = { range };

  for (int i = 0; i < 4; ++i) {
   cv::calcHist(&channels[i], 1, 0, cv::Mat(), histograms[i], 1, &histSize, &histRange);
  }

  return histograms;
 }


}