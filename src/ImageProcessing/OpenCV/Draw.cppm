

module;
#include <opencv2/opencv.hpp>
#include "../../../include/Define/DllExportMacro.hpp"

export module Draw;


export namespace ArtifactCore {

 export LIBRARY_DLL_API void drawStar5(cv::Mat& img,
  cv::Scalar edgeColor,
  int edgeThickness,
  cv::Scalar /*fillColor 無視*/,
  float scale)
 {
  if (img.empty() || scale <= 0.0f) return;

  cv::Point2f center(img.cols / 2.0f, img.rows / 2.0f);
  float radius = std::min(img.cols, img.rows) * 0.5f * scale;

  // 外周5点を取得（正五角形）
  std::vector<cv::Point> outer(5);
  for (int i = 0; i < 5; ++i) {
   float angle = CV_2PI * i / 5 - CV_PI / 2;
   float x = std::cos(angle) * radius + center.x;
   float y = std::sin(angle) * radius + center.y;
   outer[i] = cv::Point(cvRound(x), cvRound(y));
  }

  // 星型に結ぶ順番で線を描く（交差するように）
  std::vector<int> starOrder = { 0, 2, 4, 1, 3, 0 };
  for (int i = 0; i < 5; ++i) {
   const cv::Point& p1 = outer[starOrder[i]];
   const cv::Point& p2 = outer[starOrder[i + 1]];
   cv::line(img, p1, p2, edgeColor, edgeThickness, cv::LINE_AA);
  }
 }
}