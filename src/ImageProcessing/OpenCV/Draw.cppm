

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
   float angle =(float) CV_2PI * i / 5 - CV_PI / 2;
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

 void line32F(cv::Mat& img, cv::Point p1, cv::Point p2, cv::Vec4f color, int thickness = 1) {
  if (img.type() != CV_32FC4) return;

  cv::LineIterator it(img, p1, p2, 8);
  for (int i = 0; i < it.count; ++i, ++it) {
   for (int dy = -thickness / 2; dy <= thickness / 2; ++dy) {
	for (int dx = -thickness / 2; dx <= thickness / 2; ++dx) {
	 cv::Point pt = it.pos() + cv::Point(dx, dy);
	 if (pt.inside(cv::Rect(0, 0, img.cols, img.rows))) {
	  cv::Vec4f& dst = img.at<cv::Vec4f>(pt);
	  dst = color; // 必要ならαブレンドや加算合成に変更可
	 }
	}
   }
  }
 }

 void fillPoly32F(cv::Mat& img, const std::vector<cv::Point>& pts, cv::Vec4f fillColor) {
  if (img.type() != CV_32FC4) return;

  // マスク作成
  cv::Mat mask(img.size(), CV_8UC1, cv::Scalar(0));
  std::vector<std::vector<cv::Point>> pts_all{ pts };
  cv::fillPoly(mask, pts_all, 255, cv::LINE_AA);

  // 塗りつぶし（マスクされた部分を色で埋める）
  for (int y = 0; y < img.rows; ++y) {
   const uchar* mrow = mask.ptr<uchar>(y);
   cv::Vec4f* drow = img.ptr<cv::Vec4f>(y);
   for (int x = 0; x < img.cols; ++x) {
	if (mrow[x]) drow[x] = fillColor;
   }
  }
 }


 LIBRARY_DLL_API void drawStar5FloatCompatible(cv::Mat& img,
  cv::Vec4f edgeColor,
  int edgeThickness,
  cv::Vec4f fillColor,
  float scale)
 {
  if (img.empty() || img.type() != CV_32FC4 || scale <= 0.0f) return;

  cv::Point2f center(img.cols / 2.0f, img.rows / 2.0f);
  float radius = std::min(img.cols, img.rows) * 0.5f * scale;

  // 外周5点（正五角形）
  std::vector<cv::Point> outer(5);
  for (int i = 0; i < 5; ++i) {
   float angle = CV_2PI * i / 5 - CV_PI / 2;
   float x = std::cos(angle) * radius + center.x;
   float y = std::sin(angle) * radius + center.y;
   outer[i] = cv::Point(cvRound(x), cvRound(y));
  }

  // 星型（塗りつぶし用）の順番で内部三角形を構成
  std::vector<cv::Point> starFill(10);
  for (int i = 0; i < 5; ++i) {
   starFill[i * 2] = outer[i];
   starFill[i * 2 + 1] = outer[(i * 2 + 2) % 5];
  }

  // 星の塗りつぶし
  fillPoly32F(img, starFill, fillColor);

  // 星型の順に線を引く
  std::vector<int> starOrder = { 0, 2, 4, 1, 3, 0 };
  for (int i = 0; i < 5; ++i) {
   const cv::Point& p1 = outer[starOrder[i]];
   const cv::Point& p2 = outer[starOrder[i + 1]];
   line32F(img, p1, p2, edgeColor, edgeThickness);
  }
 }


}