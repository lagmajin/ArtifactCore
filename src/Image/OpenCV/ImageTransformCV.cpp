module;

#include <opencv2/opencv.hpp>
module Image:ImageTransform;


namespace ArtifactCore {

 cv::Mat convertToFloat32RGBA(const cv::Mat& input) {
  cv::Mat floatImg;

  // Step 1: 型変換（例：uint8 → float32）
  if (input.depth() != CV_32F)
   input.convertTo(floatImg, CV_32F, 1.0 / 255.0); // normalize

  else
   floatImg = input;

  // Step 2: チャンネル数を4に揃える
  cv::Mat output;
  switch (floatImg.channels()) {
  case 1: // Grayscale → RGBA
   cv::cvtColor(floatImg, output, cv::COLOR_GRAY2RGBA);
   break;
  case 3: // BGR → RGBA
   cv::cvtColor(floatImg, output, cv::COLOR_BGR2BGRA);
   break;
  case 4: // Assume already RGBA
   output = floatImg;
   break;
  default:
   throw std::runtime_error("Unsupported channel count: " + std::to_string(floatImg.channels()));
  }

  return output;
 }

}