module;
#include <vector>
#include <opencv2/opencv.hpp>

export module ArtifactCore.AI.AIColorAnalyzer;

import std;

export struct ColorStats {

  cv::Scalar meanColor;

  cv::Scalar stdColor;

  cv::Mat histogram;
};

export struct GradingParams {

  float brightness = 0.0f;

  float contrast = 1.0f;

  float saturation = 1.0f;

  float hueShift = 0.0f;
};

export class AIColorAnalyzer {

public:
  ColorStats analyze(const cv::Mat &image);
};

export class ColorGradingSuggester {

public:
  GradingParams suggest(const ColorStats &stats);
};

module :private;

ColorStats AIColorAnalyzer::analyze(const cv::Mat &image) {

  ColorStats stats;

  cv::meanStdDev(image, stats.meanColor, stats.stdColor);

  // Compute histogram for grayscale or each channel

  std::vector<cv::Mat> channels;

  cv::split(image, channels);

  int histSize = 256;

  float range[] = {0, 256};

  const float *histRange = {range};

  cv::calcHist(&channels[0], 1, 0, cv::Mat(), stats.histogram, 1, &histSize,
               &histRange);

  return stats;
}

GradingParams ColorGradingSuggester::suggest(const ColorStats &stats) {

  GradingParams params;

  // Simple heuristics

  double meanBrightness =
      (stats.meanColor[0] + stats.meanColor[1] + stats.meanColor[2]) / 3.0;

  if (meanBrightness < 100) {

    params.brightness = 0.1f;

  } else if (meanBrightness > 200) {

    params.brightness = -0.1f;
  }

  double contrast = stats.stdColor[0] + stats.stdColor[1] + stats.stdColor[2];

  if (contrast < 50) {

    params.contrast = 1.2f;
  }

  // Saturation based on color variance

  double colorVariance =
      stats.stdColor[0] * stats.stdColor[1] * stats.stdColor[2];

  if (colorVariance < 1000) {

    params.saturation = 1.1f;
  }

  return params;
}
