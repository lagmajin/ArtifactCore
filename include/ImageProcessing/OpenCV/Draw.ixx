module;
#include <opencv2/opencv.hpp>
#include "../../Define/DllExportMacro.hpp"
export module Draw;



export namespace ArtifactCore {

  LIBRARY_DLL_API void drawStar5(cv::Mat& img,
  cv::Scalar edgeColor,
  int edgeThickness,
  cv::Scalar fillColor,
  float scale);

  LIBRARY_DLL_API void drawStar5FloatCompatible(cv::Mat& img,
   cv::Vec4f edgeColor,
   int edgeThickness,
   cv::Vec4f fillColor,
   float scale);

 void drawStar6(cv::Mat& img,
  cv::Scalar edgeColor,
  int edgeThickness = 1,
  cv::Scalar fillColor = cv::Scalar(-1, -1, -1, -1),
  float scale = 1.0f);


#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <algorithm> // For std::min

 // 円周率
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief 角を丸めた正五角形を描画し、cv::Matとして返す関数
 *
 * @param img_size 画像のサイズ (cv::Size)
 * @param bg_color 画像の背景色 (cv::Scalar)
 * @param center 五角形の中心座標 (cv::Point)
 * @param size 五角形の外接円の半径
 * @param radius 角を丸める半径 (丸めが強すぎると隣接する角と重なる可能性あり)
 * @param fill_color 塗りつぶしの色 (cv::Scalar)。塗りつぶししない場合はcv::Scalar()を使用
 * @param line_color 枠線の色 (cv::Scalar)
 * @param thickness 枠線の太さ (塗りつぶしのみの場合は-1)
 * @return 描画された正五角形を含むcv::Matオブジェクト
 */
 cv::Mat drawFilledRoundedPentagon(cv::Size img_size, const cv::Scalar& bg_color,
  cv::Point center, int size, int radius,
  const cv::Scalar& fill_color, const cv::Scalar& line_color, int thickness);


}