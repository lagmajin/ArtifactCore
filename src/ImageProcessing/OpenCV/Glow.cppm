module;
#include <opencv2/opencv.hpp>
#include <random>
#include "../../../include/Define/DllExportMacro.hpp"


export module Glow;

import Image;

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

  // �}�X�N����
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

   // �F���␳�iglowColor�j
   cv::Mat colored = blurred.clone();
   for (int y = 0; y < colored.rows; ++y) {
	cv::Vec3f* row = colored.ptr<cv::Vec3f>(y);
	for (int x = 0; x < colored.cols; ++x) {
	 row[x][0] *= glowColor[0] / 255.0f;
	 row[x][1] *= glowColor[1] / 255.0f;
	 row[x][2] *= glowColor[2] / 255.0f;
	}
   }

   // �}�X�N�K�p
   if (!maskGray.empty()) {
	for (int y = 0; y < colored.rows; ++y) {
	 cv::Vec3f* row = colored.ptr<cv::Vec3f>(y);
	 const float* mrow = maskGray.ptr<float>(y);
	 for (int x = 0; x < colored.cols; ++x) {
	  row[x] *= mrow[x];
	 }
	}
   }

   // �A���t�@����
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


 LIBRARY_DLL_API cv::Mat applyGlintGlow(const cv::Mat& src, float threshold_value, int blur_radius, float intensity) {
  if (src.empty() || src.channels() != 4 || src.depth() != CV_32F) {
   std::cerr << "Input image must be CV_32FC4." << std::endl;
   return cv::Mat();
  }

  cv::Mat glow_map;
  cv::Mat bright_areas;
  cv::Mat rgb_src;
  cv::Mat alpha_src;

  // 1. RGBA����RGB�ƃA���t�@�`�����l���𕪗�
  // OCV 4.x�ȍ~�ł�split()�������I
  std::vector<cv::Mat> channels;
  cv::split(src, channels);
  rgb_src = channels[0]; // B
  cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgb_src); // BGR�Ƃ��Č���

  alpha_src = channels[3]; // �A���t�@�`�����l��

  // 2. �P�x�̒��o (RGB�摜���O���[�X�P�[���ɕϊ����A�������l����)
  // �P�x�v�Z: L = 0.2126 * R + 0.7152 * G + 0.0722 * B (Rec.709)
  // OpenCV��cvtColor(BGR2GRAY)���P�x�v�Z���g���܂����Afloat�Ȃ̂Œ���
  cv::Mat luminance_float;
  // BGR�𒼐ڃO���[�X�P�[���ɕϊ� (float�^���ێ�)
  cv::cvtColor(rgb_src, luminance_float, cv::COLOR_BGR2GRAY);

  // �������l���� (�w�肳�ꂽ臒l��薾�邢�����𒊏o)
  // THRESH_BINARY ��臒l��菬�����l��0�ɁA�傫���l��maxval(�����ł�1.0)�ɂ���
  cv::threshold(luminance_float, bright_areas, threshold_value, 1.0, cv::THRESH_BINARY);

  // bright_areas��RGB�`�����l���ɕ������āA���̉摜�̖��邢���������𒊏o
  std::vector<cv::Mat> bright_channels(3);
  bright_channels[0] = bright_areas.mul(channels[0]); // B
  bright_channels[1] = bright_areas.mul(channels[1]); // G
  bright_channels[2] = bright_areas.mul(channels[2]); // R
  cv::Mat bright_rgb_extract;
  cv::merge(bright_channels, bright_rgb_extract);


  // 3. �ڂ��� (�K�E�X�ڂ�����K�p)
  // �J�[�l���T�C�Y�͊�ł���K�v������
  int kernel_size = blur_radius * 2 + 1;
  if (kernel_size < 1) kernel_size = 1; // �ŏ�1

  cv::GaussianBlur(bright_rgb_extract, glow_map, cv::Size(kernel_size, kernel_size), 0);

  // 4. ���� (���̉摜�ɂڂ������摜�����Z)
  // AddWeighted �̓A���t�@�u�����h�����A�����ł͒��ډ��Z�Ƃ��Ďg��
  // output = src1 * alpha + src2 * beta + gamma;
  // glow_map * intensity + src
  cv::Mat output_rgb;
  cv::addWeighted(rgb_src, 1.0, glow_map, intensity, 0.0, output_rgb);

  // �ŏI�I�ȏo�͉摜��BGRA�ōČ���
  std::vector<cv::Mat> output_channels(4);
  cv::split(output_rgb, output_channels); // BGR��B,G,R�ɕ���
  output_channels[3] = alpha_src; // ���̃A���t�@�`�����l�����ė��p

  cv::Mat final_output;
  cv::merge(output_channels, final_output);

  return final_output;
 }

 LIBRARY_DLL_API void drawRandomDottedVerticalLine(cv::Mat& image, int x_center, int line_width, long num_points_to_try, unsigned int seed, const cv::Scalar& point_color, int point_size)
 {
  if (image.empty()) return;

  int width = image.cols;
  int height = image.rows;

  // 1. �c���C���̋P�x�}�b�v�i�m���}�b�v�j�𐶐�
  cv::Mat prob_map = cv::Mat::zeros(height, width, CV_32FC1); // float�^�O���[�X�P�[��

  // ���C���̒��S���痣���قǊm�����Ⴍ�Ȃ�悤�ɃO���f�[�V�������쐬
  for (int x = 0; x < width; ++x) {
   float dist_from_center = std::abs(x - x_center);
   if (dist_from_center < line_width / 2.0) {
	// ���C���̒��S�ɋ߂��قǊm������������
	// �K�E�X�֐�����`�����ȂǂŌ`��𒲐�
	prob_map.col(x).setTo(1.0f - (dist_from_center / (line_width / 2.0)) * 0.8f); // ���`�����̗�
   }
   else {
	prob_map.col(x).setTo(0.0f); // ���C���͈̔͊O��0
   }
  }
  // �����ڂ����Ċ��炩�ɂ��� (�I�v�V����)
  cv::GaussianBlur(prob_map, prob_map, cv::Size(3, 3), 0);
  // �m����0.0-1.0�ɐ��K�� (setTo�ōő�1.0�ɂȂ�悤�ɐݒ肵�Ă���Εs�v�ȏꍇ��)
  double minVal, maxVal;
  cv::minMaxLoc(prob_map, &minVal, &maxVal);
  if (maxVal > 0) {
   prob_map /= maxVal;
  }


  // 2. �Č����̂��闐���W�F�l���[�^�̏�����
  std::mt19937 gen(seed);
  std::uniform_real_distribution<> dis(0.0, 1.0);

  // 3. �_�����ƕ`��
  long points_drawn = 0;
  for (long i = 0; i < num_points_to_try; ++i) {
   // �����_���ȍ��W��I��
   int x = static_cast<int>(dis(gen) * width);
   int y = static_cast<int>(dis(gen) * height);

   if (x < 0 || x >= width || y < 0 || y >= height) continue;

   float probability = prob_map.at<float>(y, x);

   if (dis(gen) < probability) {
	cv::circle(image, cv::Point(x, y), point_size, point_color, -1, cv::LINE_AA);
	points_drawn++;
   }
  }
  std::cout << "Total points drawn: " << points_drawn << std::endl;
 }

 LIBRARY_DLL_API  cv::Mat applyVerticalGlow(const cv::Mat& src, float threshold_value, int vertical_blur_radius, float intensity)
 {
  cv::Mat processed_src = src; // �܂��͓��͉摜�����̂܂܎g�p

  // ���͉摜��CV_32FC4�łȂ��ꍇ�A�ϊ������݂�
  if (src.empty() || src.channels() != 4 || src.depth() != CV_32F) {
   std::cerr << "Input image is not CV_32FC4. Attempting conversion..." << std::endl;
   processed_src = convertToFloat32RGBA(src); // �w���p�[�֐��ŕϊ�
   if (processed_src.empty()) {
	// �ϊ��Ɏ��s�����ꍇ
	std::cerr << "Failed to convert input image to CV_32FC4. Aborting." << std::endl;
	return cv::Mat();
   }
  }

  // ��������A�ȍ~�̏����ł� `processed_src` ���g�p����
  // `processed_src` �͏�� CV_32FC4 �ł��邱�Ƃ��ۏ؂����
  cv::Mat glow_map;
  cv::Mat bright_areas;
  cv::Mat rgb_src;   // �����BGR���ɂȂ�
  cv::Mat alpha_src;

  // 1. BGRA����BGR�ƃA���t�@�`�����l���𕪗�
  std::vector<cv::Mat> channels;
  cv::split(processed_src, channels); // processed_src �� BGRA (B, G, R, A) �̏��ł����
  // channels[0]=B, channels[1]=G, channels[2]=R, channels[3]=A

  // BGR�Ƃ��Č����ichannels[0], channels[1], channels[2] �� B, G, R�j
  cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgb_src); // ����� rgb_src �� BGR���ɂȂ�
  alpha_src = channels[3]; // �A���t�@�`�����l��

  // 2. �P�x�̒��o (BGR�摜���O���[�X�P�[���ɕϊ����A�������l����)
  cv::Mat luminance_float;
  // �����͌��X COLOR_BGR2GRAY �Ȃ̂ŏC���s�v (rgb_src �� BGR��)
  cv::cvtColor(rgb_src, luminance_float, cv::COLOR_BGR2GRAY);

  cv::threshold(luminance_float, bright_areas, threshold_value, 1.0, cv::THRESH_BINARY);

  std::vector<cv::Mat> bright_channels(3);
  // bright_areas ��1�`�����l���Achannels[0] (B), channels[1] (G), channels[2] (R) �����ꂼ��1�`�����l��
  bright_channels[0] = bright_areas.mul(channels[0]); // B�`�����l���ɓK�p
  bright_channels[1] = bright_areas.mul(channels[1]); // G�`�����l���ɓK�p
  bright_channels[2] = bright_areas.mul(channels[2]); // R�`�����l���ɓK�p
  cv::Mat bright_rgb_extract;
  cv::merge(bright_channels, bright_rgb_extract); // bright_rgb_extract �� BGR��

  // 3. �c�����݂̂̂ڂ��� (Gaussian Blur with asymmetric kernel)
  int kernel_size_y = vertical_blur_radius * 2 + 1;
  if (kernel_size_y < 1) kernel_size_y = 1;

  int kernel_size_x = vertical_blur_radius * 2 + 1; // �c�Ɠ������W�b�N�Ōv�Z
  if (kernel_size_x < 1) kernel_size_x = 1;

  cv::Mat glow_map_vertical;
  cv::Mat glow_map_horizontal;

  cv::GaussianBlur(bright_rgb_extract, glow_map_vertical, cv::Size(1, kernel_size_y), 0); // bright_rgb_extract �� BGR��

  cv::GaussianBlur(bright_rgb_extract, glow_map_horizontal, cv::Size(kernel_size_x, 1), 0);

  cv::addWeighted(glow_map_vertical, 1.0, glow_map_horizontal, 1.0, 0.0, glow_map); // �P���ɉ��Z

  // 4. ���� (���̉摜�ɂڂ������摜�����Z)
  cv::Mat output_rgb;
  cv::addWeighted(rgb_src, 1.0,glow_map, intensity, 0.0, output_rgb); // rgb_src �� BGR���Aglow_map �� BGR��



  // �ŏI�I�ȏo�͉摜��BGRA�ōČ���
  std::vector<cv::Mat> output_channels_to_merge; // �V�����x�N�g�����`

  // output_rgb �� BGR����3�`�����l���摜�Ȃ̂ŁA����𕪊�
  std::vector<cv::Mat> bgr_channels_from_output;
  cv::split(output_rgb, bgr_channels_from_output); // output_rgb��BGR��3�`�����l���ɕ���

  // ��������B, G, R�`�����l����V�����x�N�g���ɒǉ�
  output_channels_to_merge.push_back(bgr_channels_from_output[0]); // B�`�����l��
  output_channels_to_merge.push_back(bgr_channels_from_output[1]); // G�`�����l��
  output_channels_to_merge.push_back(bgr_channels_from_output[2]); // R�`�����l��

  // �Ō�ɃA���t�@�`�����l����ǉ�
  output_channels_to_merge.push_back(alpha_src); // ���̃A���t�@�`�����l�����ė��p

  cv::Mat final_output;
  // output_channels_to_merge ���g����BGRA�Ƃ��Č���
  cv::merge(output_channels_to_merge, final_output);

  return final_output;
 }

}