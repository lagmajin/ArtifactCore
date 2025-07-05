

module;
#include <opencv2/opencv.hpp>
#include <OpenImageIO/fmath.h>

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
   float angle =(float) CV_2PI * i / 5 - CV_PI / 2;
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

 LIBRARY_DLL_API cv::Mat drawFilledRoundedPentagon(cv::Size img_size, const cv::Scalar& bg_color, cv::Point center, int size, int radius, const cv::Scalar& fill_color, const cv::Scalar& line_color, int thickness)
 {
  // 新しい画像を背景色で初期化
  cv::Mat img = cv::Mat::zeros(img_size, CV_8UC3);
  img = bg_color;

  std::vector<cv::Point> vertices;
  // 正五角形の通常の頂点を計算
  for (int i = 0; i < 5; ++i) {
   // 頂点を上向きから開始 (270度 = -90度)
   double angle_deg = 270.0 + i * 72.0;
   double angle_rad = angle_deg * M_PI / 180.0;

   int x = static_cast<int>(center.x + size * std::cos(angle_rad));
   int y = static_cast<int>(center.y + size * std::sin(angle_rad));
   vertices.push_back(cv::Point(x, y));
  }

  std::vector<cv::Point> polygonPoints; // 直線部分の点を格納
  std::vector<std::pair<cv::Point, double>> arcInfos; // 円弧の中心と開始・終了角度を格納 (簡略版)

  for (int i = 0; i < 5; ++i) {
   cv::Point currentVertex = vertices[i];
   cv::Point prevVertex = vertices[(i - 1 + 5) % 5];
   cv::Point nextVertex = vertices[(i + 1) % 5];

   cv::Point2d vecPrev = cv::Point2d(prevVertex.x - currentVertex.x, prevVertex.y - currentVertex.y);
   cv::Point2d vecNext = cv::Point2d(nextVertex.x - currentVertex.x, nextVertex.y - currentVertex.y);

   double normPrev = std::sqrt(vecPrev.x * vecPrev.x + vecPrev.y * vecPrev.y);
   double normNext = std::sqrt(vecNext.x * vecNext.x + vecNext.y * vecNext.y);

   if (normPrev == 0 || normNext == 0) {
	// ゼロ割り回避
	continue;
   }

   cv::Point2d unitVecPrev = cv::Point2d(vecPrev.x / normPrev, vecPrev.y / normPrev);
   cv::Point2d unitVecNext = cv::Point2d(vecNext.x / normNext, vecNext.y / normNext);

   double segLenPrev = std::min(static_cast<double>(radius), normPrev / 2.0);
   double segLenNext = std::min(static_cast<double>(radius), normNext / 2.0);

   cv::Point point1_line = cv::Point(static_cast<int>(currentVertex.x + unitVecPrev.x * segLenPrev),
	static_cast<int>(currentVertex.y + unitVecPrev.y * segLenPrev));
   cv::Point point2_line = cv::Point(static_cast<int>(currentVertex.x + unitVecNext.x * segLenNext),
	static_cast<int>(currentVertex.y + unitVecNext.y * segLenNext));

   polygonPoints.push_back(point2_line); // 直線の開始点

   // --- 円弧の中心と角度の計算（簡略化版） ---
   // ここがより複雑な幾何学計算を必要とする部分です。
   // 正確な丸めには、角の二等分線を利用して円弧の中心を求め、
   // その中心からpoint1_lineとpoint2_lineが成す角度を計算する必要があります。
   // 例として、近似的な円弧の情報を格納します。

   // 仮の円弧の中心（角の頂点に近い場所）
   cv::Point arc_center_approx = currentVertex;

   // point1_lineとpoint2_lineが円の中心からなす角度を計算
   // atan2(y, x) はベクトル(x,y)の角度をラジアンで返す
   double angle1 = std::atan2(point1_line.y - arc_center_approx.y, point1_line.x - arc_center_approx.x) * 180.0 / M_PI;
   double angle2 = std::atan2(point2_line.y - arc_center_approx.y, point2_line.x - arc_center_approx.x) * 180.0 / M_PI;

   // 角度が負になる場合があるので調整
   if (angle1 < 0) angle1 += 360;
   if (angle2 < 0) angle2 += 360;

   // 円弧を描画するために、開始角度と終了角度を調整
   // これは非常に単純な実装であり、正確な丸めではない可能性があります
   double startAngle = angle2;
   double endAngle = angle1;

   // 必要に応じて角度を正規化 (0-360度)
   if (startAngle > endAngle) {
	std::swap(startAngle, endAngle); // 小さい方が開始角度
   }
   // もし270度から20度のようにまたぐ場合、endAngleに360度足すなどの処理が必要
   // ここでは単純な角度範囲で処理

   arcInfos.push_back({ arc_center_approx, startAngle });
   arcInfos.push_back({ arc_center_approx, endAngle }); // ダミーとして終点角度も保存

   polygonPoints.push_back(point1_line); // 直線の終了点
  }

  // 塗りつぶしの場合、ポリゴンと円弧で囲まれた領域を生成
  // cv::fillPoly()を使うための点のリストを準備
  std::vector<std::vector<cv::Point>> fill_pts(1);
  for (size_t i = 0; i < polygonPoints.size(); ++i) {
   fill_pts[0].push_back(polygonPoints[i]);
   // ここに円弧の点を追加することもできるが、複雑になるため直線近似
  }

  if (fill_color[0] != 0 || fill_color[1] != 0 || fill_color[2] != 0 || fill_color[3] != 0) { // 色が設定されていれば塗りつぶし
   cv::fillPoly(img, fill_pts, fill_color, cv::LINE_AA);
  }

  // 枠線の描画
  if (thickness != -1) {
   for (size_t i = 0; i < polygonPoints.size(); ++i) {
	cv::line(img, polygonPoints[i], polygonPoints[(i + 1) % polygonPoints.size()], line_color, thickness, cv::LINE_AA);
   }

   // 円弧の描画 (これは簡略化されたもので、正確な角丸めではない可能性があります)
   // 各頂点に描画する円弧は、本来はpoint1_lineとpoint2_lineの間を補間する
   for (int i = 0; i < 5; ++i) {
	cv::Point arcCenter = vertices[i]; // ここを正確に計算する必要がある
	cv::ellipse(img, arcCenter, cv::Size(radius, radius), 0,
	 arcInfos[i * 2].second, arcInfos[i * 2 + 1].second, line_color, thickness, cv::LINE_AA);
   }
  }

  return img;
 }





}