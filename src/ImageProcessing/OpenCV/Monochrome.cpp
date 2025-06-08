
module;

#include <opencv2/opencv.hpp>
#include "../../../include/Define/DllExportMacro.hpp"
module ImageProcessing:Monochrome;

import std;




namespace ArtifactCore {

 float srgbToLinear(float c) {
  if (c <= 0.04045f) return c / 12.92f;
  return std::pow((c + 0.055f) / 1.055f, 2.4f);
 }

 // Linear RGB → sRGB
 float linearToSrgb(float c) {
  if (c <= 0.0031308f) return 12.92f * c;
  return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
 }

 // 光学的に正しいモノクロ変換（BT.709）
 LIBRARY_DLL_API cv::Mat convertToPhysicallyCorrectGrayscale(const cv::Mat& src_bgr) {
  CV_Assert(src_bgr.type() == CV_8UC3);

  cv::Mat gray(src_bgr.size(), CV_8UC1);

  for (int y = 0; y < src_bgr.rows; ++y) {
   for (int x = 0; x < src_bgr.cols; ++x) {
	cv::Vec3b bgr = src_bgr.at<cv::Vec3b>(y, x);

	// BGR → RGB正規化（0-1）
	float r_srgb = bgr[2] / 255.0f;
	float g_srgb = bgr[1] / 255.0f;
	float b_srgb = bgr[0] / 255.0f;

	// sRGB → リニア
	float r_lin = srgbToLinear(r_srgb);
	float g_lin = srgbToLinear(g_srgb);
	float b_lin = srgbToLinear(b_srgb);

	// BT.709加重平均
	float y_lin = 0.2126f * r_lin + 0.7152f * g_lin + 0.0722f * b_lin;

	// リニア → sRGB
	float y_srgb = linearToSrgb(y_lin);

	// 0-255に変換
	gray.at<uchar>(y, x) = static_cast<uchar>(std::round(y_srgb * 255.0f));
   }
  }

  return gray;
 }

 void toLuminanceGrayRGBA(const cv::Mat& src, cv::Mat& dst)
 {
  CV_Assert(src.type() == CV_8UC4); // BGRA

  cv::Mat floatSrc;
  src.convertTo(floatSrc, CV_32FC4);

  // 光学的モノクロ変換マトリクス（BT.709）
  const cv::Matx<float, 4, 4> transformMatrix = {
		0.0722f, 0.7152f, 0.2126f, 0.0f,
		0.0722f, 0.7152f, 0.2126f, 0.0f,
		0.0722f, 0.7152f, 0.2126f, 0.0f,
		0.0f,    0.0f,    0.0f,    1.0f
  };

  cv::Mat floatDst;
  cv::transform(floatSrc, floatDst, transformMatrix);

  floatDst.convertTo(dst, CV_8UC4);
 }


}