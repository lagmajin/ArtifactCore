module;
#include <random>
#include <opencv2/opencv.hpp>
#include "../../Define/DllExportMacro.hpp"
export module Glow;



export namespace ArtifactCore {

 export LIBRARY_DLL_API void applySimpleGlow(
  const cv::Mat& src,               // 入力画像（BGR or BGRA）
  const cv::Mat& mask,              // グロー対象マスク（CV_8UC1 or empty）
  cv::Mat& dst,                     // 出力先

  const cv::Scalar& glowColor,      // グロー色（例：白 / 任意色）
  float glowGain = 1.0f,            // 全体グロー強度係数

  int layerCount = 4,               // 重ねるレイヤ数（例：4）
  float baseSigma = 5.0f,           // 最初のブラー半径
  float sigmaGrowth = 1.8f,         // ブラー半径の増加比
  float baseAlpha = 0.3f,           // 最初の合成比率
  float alphaFalloff = 0.6f,        // 合成比の減衰（指数的）

  bool additiveBlend = true,        // 合成するか否か（src + glow）
  bool linearSpace = true           // 線形色空間で処理
 );

 LIBRARY_DLL_API cv::Mat applyGlintGlow(const cv::Mat& src, float threshold_value, int blur_radius, float intensity);


 LIBRARY_DLL_API void drawRandomDottedVerticalLine(cv::Mat& image, int x_center, int line_width,
  long num_points_to_try, unsigned int seed,
  const cv::Scalar& point_color, int point_size);


 LIBRARY_DLL_API cv::Mat applyVerticalGlow(const cv::Mat& src, float threshold_value, int vertical_blur_radius, float intensity);

}