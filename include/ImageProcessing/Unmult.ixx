module;
#include <vector>
#include <opencv2/opencv.hpp>

export module ArtifactCore.ImageProcessing.Unmult;

export cv::Mat unmultiplyAlpha(const cv::Mat &src);

module :private;

cv::Mat unmultiplyAlpha(const cv::Mat &src) {

  cv::Mat result = src.clone();

  if (result.channels() != 4)
    return result;

  std::vector<cv::Mat> channels(4);

  cv::split(result, channels);

  cv::Mat alpha = channels[3];

  // Avoid division by zero

  cv::Mat alphaSafe = alpha + 1e-6;

  for (int i = 0; i < 3; ++i) {

    cv::divide(channels[i], alphaSafe, channels[i]);
  }

  cv::merge(channels, result);

  return result;
}
