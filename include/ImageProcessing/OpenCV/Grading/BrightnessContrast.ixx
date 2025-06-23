
module;
#include <opencv2/opencv.hpp>
export module BrightnessContrast;

export namespace ArtifactCore
{

 cv::Mat adjustBrightnessContrastFloat_RGBA(const cv::Mat& srcFloat, float brightness, float contrast) {
  CV_Assert(srcFloat.type() == CV_32FC4); // RGBA float

  // アルファチャンネルをそのまま保持しつつ、RGBだけ補正
  cv::Mat dst(srcFloat.size(), srcFloat.type());
  std::vector<cv::Mat> channels(4);
  cv::split(srcFloat, channels);

  for (int i = 0; i < 3; ++i) { // R, G, Bのみ補正
   channels[i] = (channels[i] - 0.5f) * contrast + 0.5f + brightness;
   cv::min(channels[i], 1.0f, channels[i]);
   cv::max(channels[i], 0.0f, channels[i]);
  }

  // channels[3]（アルファ）はそのまま
  cv::merge(channels, dst);
  return dst;
 }








};