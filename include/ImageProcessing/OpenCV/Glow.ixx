module;
#include <random>
#include <opencv2/opencv.hpp>
#include "../../Define/DllExportMacro.hpp"
export module Draw;



export namespace ArtifactCore {

 export LIBRARY_DLL_API void applySimpleGlow(
  const cv::Mat& src,               // ���͉摜�iBGR or BGRA�j
  const cv::Mat& mask,              // �O���[�Ώۃ}�X�N�iCV_8UC1 or empty�j
  cv::Mat& dst,                     // �o�͐�

  const cv::Scalar& glowColor,      // �O���[�F�i��F�� / �C�ӐF�j
  float glowGain = 1.0f,            // �S�̃O���[���x�W��

  int layerCount = 4,               // �d�˂郌�C�����i��F4�j
  float baseSigma = 5.0f,           // �ŏ��̃u���[���a
  float sigmaGrowth = 1.8f,         // �u���[���a�̑�����
  float baseAlpha = 0.3f,           // �ŏ��̍����䗦
  float alphaFalloff = 0.6f,        // ������̌����i�w���I�j

  bool additiveBlend = true,        // �������邩�ۂ��isrc + glow�j
  bool linearSpace = true           // ���`�F��Ԃŏ���
 );

 LIBRARY_DLL_API cv::Mat applyGlintGlow(const cv::Mat& src, float threshold_value, int blur_radius, float intensity);


 LIBRARY_DLL_API void drawRandomDottedVerticalLine(cv::Mat& image, int x_center, int line_width,
  long num_points_to_try, unsigned int seed,
  const cv::Scalar& point_color, int point_size);


 LIBRARY_DLL_API cv::Mat applyVerticalGlow(const cv::Mat& src, float threshold_value, int vertical_blur_radius, float intensity);

}