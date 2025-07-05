

module;
#include <opencv2/opencv.hpp>
#include <OpenImageIO/fmath.h>

#include "../../../include/Define/DllExportMacro.hpp"

export module Draw;


export namespace ArtifactCore {

 export LIBRARY_DLL_API void drawStar5(cv::Mat& img,
  cv::Scalar edgeColor,
  int edgeThickness,
  cv::Scalar /*fillColor ����*/,
  float scale)
 {
  if (img.empty() || scale <= 0.0f) return;

  cv::Point2f center(img.cols / 2.0f, img.rows / 2.0f);
  float radius = std::min(img.cols, img.rows) * 0.5f * scale;

  // �O��5�_���擾�i���܊p�`�j
  std::vector<cv::Point> outer(5);
  for (int i = 0; i < 5; ++i) {
   float angle =(float) CV_2PI * i / 5 - CV_PI / 2;
   float x = std::cos(angle) * radius + center.x;
   float y = std::sin(angle) * radius + center.y;
   outer[i] = cv::Point(cvRound(x), cvRound(y));
  }

  // ���^�Ɍ��ԏ��ԂŐ���`���i��������悤�Ɂj
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
	  dst = color; // �K�v�Ȃ烿�u�����h����Z�����ɕύX��
	 }
	}
   }
  }
 }

 void fillPoly32F(cv::Mat& img, const std::vector<cv::Point>& pts, cv::Vec4f fillColor) {
  if (img.type() != CV_32FC4) return;

  // �}�X�N�쐬
  cv::Mat mask(img.size(), CV_8UC1, cv::Scalar(0));
  std::vector<std::vector<cv::Point>> pts_all{ pts };
  cv::fillPoly(mask, pts_all, 255, cv::LINE_AA);

  // �h��Ԃ��i�}�X�N���ꂽ������F�Ŗ��߂�j
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

  // �O��5�_�i���܊p�`�j
  std::vector<cv::Point> outer(5);
  for (int i = 0; i < 5; ++i) {
   float angle =(float) CV_2PI * i / 5 - CV_PI / 2;
   float x = std::cos(angle) * radius + center.x;
   float y = std::sin(angle) * radius + center.y;
   outer[i] = cv::Point(cvRound(x), cvRound(y));
  }

  // ���^�i�h��Ԃ��p�j�̏��Ԃœ����O�p�`���\��
  std::vector<cv::Point> starFill(10);
  for (int i = 0; i < 5; ++i) {
   starFill[i * 2] = outer[i];
   starFill[i * 2 + 1] = outer[(i * 2 + 2) % 5];
  }

  // ���̓h��Ԃ�
  fillPoly32F(img, starFill, fillColor);

  // ���^�̏��ɐ�������
  std::vector<int> starOrder = { 0, 2, 4, 1, 3, 0 };
  for (int i = 0; i < 5; ++i) {
   const cv::Point& p1 = outer[starOrder[i]];
   const cv::Point& p2 = outer[starOrder[i + 1]];
   line32F(img, p1, p2, edgeColor, edgeThickness);
  }
 }

 LIBRARY_DLL_API cv::Mat drawFilledRoundedPentagon(cv::Size img_size, const cv::Scalar& bg_color, cv::Point center, int size, int radius, const cv::Scalar& fill_color, const cv::Scalar& line_color, int thickness)
 {
  // �V�����摜��w�i�F�ŏ�����
  cv::Mat img = cv::Mat::zeros(img_size, CV_8UC3);
  img = bg_color;

  std::vector<cv::Point> vertices;
  // ���܊p�`�̒ʏ�̒��_���v�Z
  for (int i = 0; i < 5; ++i) {
   // ���_�����������J�n (270�x = -90�x)
   double angle_deg = 270.0 + i * 72.0;
   double angle_rad = angle_deg * M_PI / 180.0;

   int x = static_cast<int>(center.x + size * std::cos(angle_rad));
   int y = static_cast<int>(center.y + size * std::sin(angle_rad));
   vertices.push_back(cv::Point(x, y));
  }

  std::vector<cv::Point> polygonPoints; // ���������̓_���i�[
  std::vector<std::pair<cv::Point, double>> arcInfos; // �~�ʂ̒��S�ƊJ�n�E�I���p�x���i�[ (�ȗ���)

  for (int i = 0; i < 5; ++i) {
   cv::Point currentVertex = vertices[i];
   cv::Point prevVertex = vertices[(i - 1 + 5) % 5];
   cv::Point nextVertex = vertices[(i + 1) % 5];

   cv::Point2d vecPrev = cv::Point2d(prevVertex.x - currentVertex.x, prevVertex.y - currentVertex.y);
   cv::Point2d vecNext = cv::Point2d(nextVertex.x - currentVertex.x, nextVertex.y - currentVertex.y);

   double normPrev = std::sqrt(vecPrev.x * vecPrev.x + vecPrev.y * vecPrev.y);
   double normNext = std::sqrt(vecNext.x * vecNext.x + vecNext.y * vecNext.y);

   if (normPrev == 0 || normNext == 0) {
	// �[��������
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

   polygonPoints.push_back(point2_line); // �����̊J�n�_

   // --- �~�ʂ̒��S�Ɗp�x�̌v�Z�i�ȗ����Łj ---
   // ��������蕡�G�Ȋ􉽊w�v�Z��K�v�Ƃ��镔���ł��B
   // ���m�Ȋۂ߂ɂ́A�p�̓񓙕����𗘗p���ĉ~�ʂ̒��S�����߁A
   // ���̒��S����point1_line��point2_line�������p�x���v�Z����K�v������܂��B
   // ��Ƃ��āA�ߎ��I�ȉ~�ʂ̏����i�[���܂��B

   // ���̉~�ʂ̒��S�i�p�̒��_�ɋ߂��ꏊ�j
   cv::Point arc_center_approx = currentVertex;

   // point1_line��point2_line���~�̒��S����Ȃ��p�x���v�Z
   // atan2(y, x) �̓x�N�g��(x,y)�̊p�x�����W�A���ŕԂ�
   double angle1 = std::atan2(point1_line.y - arc_center_approx.y, point1_line.x - arc_center_approx.x) * 180.0 / M_PI;
   double angle2 = std::atan2(point2_line.y - arc_center_approx.y, point2_line.x - arc_center_approx.x) * 180.0 / M_PI;

   // �p�x�����ɂȂ�ꍇ������̂Œ���
   if (angle1 < 0) angle1 += 360;
   if (angle2 < 0) angle2 += 360;

   // �~�ʂ�`�悷�邽�߂ɁA�J�n�p�x�ƏI���p�x�𒲐�
   // ����͔��ɒP���Ȏ����ł���A���m�Ȋۂ߂ł͂Ȃ��\��������܂�
   double startAngle = angle2;
   double endAngle = angle1;

   // �K�v�ɉ����Ċp�x�𐳋K�� (0-360�x)
   if (startAngle > endAngle) {
	std::swap(startAngle, endAngle); // �����������J�n�p�x
   }
   // ����270�x����20�x�̂悤�ɂ܂����ꍇ�AendAngle��360�x�����Ȃǂ̏������K�v
   // �����ł͒P���Ȋp�x�͈͂ŏ���

   arcInfos.push_back({ arc_center_approx, startAngle });
   arcInfos.push_back({ arc_center_approx, endAngle }); // �_�~�[�Ƃ��ďI�_�p�x���ۑ�

   polygonPoints.push_back(point1_line); // �����̏I���_
  }

  // �h��Ԃ��̏ꍇ�A�|���S���Ɖ~�ʂň͂܂ꂽ�̈�𐶐�
  // cv::fillPoly()���g�����߂̓_�̃��X�g������
  std::vector<std::vector<cv::Point>> fill_pts(1);
  for (size_t i = 0; i < polygonPoints.size(); ++i) {
   fill_pts[0].push_back(polygonPoints[i]);
   // �����ɉ~�ʂ̓_��ǉ����邱�Ƃ��ł��邪�A���G�ɂȂ邽�ߒ����ߎ�
  }

  if (fill_color[0] != 0 || fill_color[1] != 0 || fill_color[2] != 0 || fill_color[3] != 0) { // �F���ݒ肳��Ă���Γh��Ԃ�
   cv::fillPoly(img, fill_pts, fill_color, cv::LINE_AA);
  }

  // �g���̕`��
  if (thickness != -1) {
   for (size_t i = 0; i < polygonPoints.size(); ++i) {
	cv::line(img, polygonPoints[i], polygonPoints[(i + 1) % polygonPoints.size()], line_color, thickness, cv::LINE_AA);
   }

   // �~�ʂ̕`�� (����͊ȗ������ꂽ���̂ŁA���m�Ȋp�ۂ߂ł͂Ȃ��\��������܂�)
   // �e���_�ɕ`�悷��~�ʂ́A�{����point1_line��point2_line�̊Ԃ��Ԃ���
   for (int i = 0; i < 5; ++i) {
	cv::Point arcCenter = vertices[i]; // �����𐳊m�Ɍv�Z����K�v������
	cv::ellipse(img, arcCenter, cv::Size(radius, radius), 0,
	 arcInfos[i * 2].second, arcInfos[i * 2 + 1].second, line_color, thickness, cv::LINE_AA);
   }
  }

  return img;
 }





}