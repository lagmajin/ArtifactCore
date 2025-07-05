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

 // �~����
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief �p���ۂ߂����܊p�`��`�悵�Acv::Mat�Ƃ��ĕԂ��֐�
 *
 * @param img_size �摜�̃T�C�Y (cv::Size)
 * @param bg_color �摜�̔w�i�F (cv::Scalar)
 * @param center �܊p�`�̒��S���W (cv::Point)
 * @param size �܊p�`�̊O�ډ~�̔��a
 * @param radius �p���ۂ߂锼�a (�ۂ߂���������Ɨאڂ���p�Əd�Ȃ�\������)
 * @param fill_color �h��Ԃ��̐F (cv::Scalar)�B�h��Ԃ����Ȃ��ꍇ��cv::Scalar()���g�p
 * @param line_color �g���̐F (cv::Scalar)
 * @param thickness �g���̑��� (�h��Ԃ��݂̂̏ꍇ��-1)
 * @return �`�悳�ꂽ���܊p�`���܂�cv::Mat�I�u�W�F�N�g
 */
 cv::Mat drawFilledRoundedPentagon(cv::Size img_size, const cv::Scalar& bg_color,
  cv::Point center, int size, int radius,
  const cv::Scalar& fill_color, const cv::Scalar& line_color, int thickness);


}