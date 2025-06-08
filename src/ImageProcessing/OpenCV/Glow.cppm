module;
#include <opencv2/opencv.hpp>
#include "../../../include/Define/DllExportMacro.hpp"


export module Glow;

export namespace ArtifactCore {

 export LIBRARY_DLL_API void applySimpleGlow(
  const cv::Mat& src,
  const cv::Mat& mask,
  cv::Mat& dst,
  const cv::Scalar& glowColor,
  float glowGain,
  int layerCount,
  float baseSigma,
  float sigmaGrowth,
  float baseAlpha,
  float alphaFalloff,
  bool additiveBlend,
  bool linearSpace
 ) {
  if (src.empty()) return;

  CV_Assert(src.type() == CV_8UC3 || src.type() == CV_8UC4);

  cv::Mat glowAccum = cv::Mat::zeros(src.size(), CV_32FC3);
  cv::Mat srcFloat;
  src.convertTo(srcFloat, CV_32FC3, linearSpace ? 1.0 / 255.0 : 1.0);

  // マスク処理
  cv::Mat maskGray;
  if (!mask.empty()) {
   if (mask.channels() == 1)
	maskGray = mask;
   else
	cv::cvtColor(mask, maskGray, cv::COLOR_BGR2GRAY);
   maskGray.convertTo(maskGray, CV_32FC1, 1.0 / 255.0);
  }

  for (int i = 0; i < layerCount; ++i) {
   float sigma =(float) baseSigma * std::pow(sigmaGrowth, i);
   float alpha = baseAlpha * std::pow(alphaFalloff, i);

   cv::Mat blurred;
   cv::GaussianBlur(srcFloat, blurred, cv::Size(), sigma, sigma);

   // 色調補正（glowColor）
   cv::Mat colored = blurred.clone();
   for (int y = 0; y < colored.rows; ++y) {
	cv::Vec3f* row = colored.ptr<cv::Vec3f>(y);
	for (int x = 0; x < colored.cols; ++x) {
	 row[x][0] *= glowColor[0] / 255.0f;
	 row[x][1] *= glowColor[1] / 255.0f;
	 row[x][2] *= glowColor[2] / 255.0f;
	}
   }

   // マスク適用
   if (!maskGray.empty()) {
	for (int y = 0; y < colored.rows; ++y) {
	 cv::Vec3f* row = colored.ptr<cv::Vec3f>(y);
	 const float* mrow = maskGray.ptr<float>(y);
	 for (int x = 0; x < colored.cols; ++x) {
	  row[x] *= mrow[x];
	 }
	}
   }

   // アルファ合成
   glowAccum += colored * alpha;
  }

  glowAccum *= glowGain;
  cv::Mat glowFinal;
  glowAccum.convertTo(glowFinal, CV_8UC3, linearSpace ? 255.0 : 1.0);

  if (additiveBlend) {
   cv::Mat base;
   if (src.channels() == 4)
	cv::cvtColor(src, base, cv::COLOR_BGRA2BGR);
   else
	base = src;

   cv::add(base, glowFinal, dst);
  }
  else {
   dst = glowFinal;
  }
 }






}