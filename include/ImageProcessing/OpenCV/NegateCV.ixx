module;
#include <opencv2/opencv.hpp>
export module ImageProcessing:NegateCV;

export namespace ArtifactCore
{
 cv::Mat negateBgraFloat32Image(const cv::Mat& input_image) {
  // ���͉摜�̃`�F�b�N
  if (input_image.empty()) {
   std::cerr << "Error: Input image is empty." << std::endl;
   return cv::Mat();
  }

  // �摜��BGRA (4�`�����l��) �ɕϊ�
  // imread�̓f�t�H���g��BGR�œǂݍ��ނ��߁ARGBA/BGRA�摜��IMREAD_UNCHANGED�œǂݍ���
  // �������A���͂��m����BGRA�ł���ۏ؂͂Ȃ����߁A�ϊ�����������
  cv::Mat bgra_image;
  if (input_image.channels() == 3) {
   // BGR����BGRA�֕ϊ� (�A���t�@�`�����l����255�Œǉ�)
   cv::cvtColor(input_image, bgra_image, cv::COLOR_BGR2BGRA);
  }
  else if (input_image.channels() == 4) {
   // ���ł�4�`�����l���ł���΂��̂܂�
   input_image.copyTo(bgra_image);
  }
  else {
   std::cerr << "Error: Unsupported number of channels: " << input_image.channels() << std::endl;
   return cv::Mat();
  }

  // float32�^ (0.0 - 1.0) �ɕϊ�
  cv::Mat bgra_float;
  bgra_image.convertTo(bgra_float, CV_32FC4, 1.0 / 255.0);

  // �l�K�|�W���]����
  // �e�`�����l�� (B, G, R) �ɑ΂��� 1.0 - p ���v�Z
  // �A���t�@�`�����l���͂��̂܂� (�����ł̓C���f�b�N�X3)
  cv::Mat negated_bgra_float = bgra_float.clone(); // �N���[�����Č��̉摜��ێ�

  // �摜�̗v�f�ɃA�N�Z�X���Ĕ��]
  // �s�N�Z�����Ƃ̑���Ȃ̂ŁA�������I��OpenCV�̊֐��𗘗p
  // cv::split �Ń`�����l���𕪊����AB, G, R �`�����l���̂ݔ��]
  std::vector<cv::Mat> channels;
  cv::split(negated_bgra_float, channels);

  // B, G, R �`�����l���𔽓] (channels[0], channels[1], channels[2])
  // 1.0 - channel_mat �̌v�Z
  channels[0] = 1.0 - channels[0]; // Blue
  channels[1] = 1.0 - channels[1]; // Green
  channels[2] = 1.0 - channels[2]; // Red
  // channels[3] (Alpha) �͂��̂܂�

  cv::merge(channels, negated_bgra_float);

  return negated_bgra_float;
 }










};