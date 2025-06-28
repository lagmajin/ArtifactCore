
module ;
#include <Halide.h>
#include <opencv2/opencv.hpp>
#include "../../Define/DllExportMacro.hpp"

export module HalideTest;

import std;

export namespace ArtifactCore
{
 namespace Internal {
  inline void halide_no_op_free(void* /*user_context*/, void* /*ptr*/) {
   // No-op free function
  }
 }



 using namespace Halide;
 LIBRARY_DLL_API cv::Mat process_bgra_mat_with_halide(const cv::Mat& input_cv_mat) {
  // 1. ����cv::Mat�̌���
  if (input_cv_mat.empty()) {
   std::cerr << "�G���[: ����cv::Mat����ł��B" << std::endl;
   return cv::Mat(); // ���Mat��Ԃ�
  }
  if (input_cv_mat.type() != CV_32FC4) {
   std::cerr << "�G���[: ����cv::Mat�̌^��CV_32FC4�ł͂���܂���B���݂̌^: "
	<< input_cv_mat.type() << std::endl;
   return cv::Mat(); // ���Mat��Ԃ�
  }

  int width = input_cv_mat.cols;
  int height = input_cv_mat.rows;
  int channels = input_cv_mat.channels(); // ����͏��4

  // 2. OpenCV Mat��Halide::Buffer�ɕϊ�
  // make_interleaved�̓C���^�[���[�u���ꂽ�`�����l���f�[�^�������̂ɓK���Ă��܂��B
  // (x, y, c) �̏��ŃA�N�Z�X�ł���悤�ɂȂ�܂��B
  Buffer<float> input_halide_buffer = Buffer<float>::make_interleaved(
   (float*)input_cv_mat.data, // Mat�̐��f�[�^�|�C���^
   width,                     // ��
   height,                    // ����
   channels                   // �`�����l���� (4)
  );

  // 3. Halide�p�C�v���C���̒�`
  Var x("x"), y("y"), c("c"); // ���W�ƃ`�����l���̕ϐ�

  // ����Func: input_halide_buffer��Func�Ƃ��ă��b�v
  Func input_func("input_func");
  input_func(x, y, c) = input_halide_buffer(x, y, c);

  // �o��Func: �������ʂ��`
  Func output_func("output_func");

  // �ȒP�ȏ���: BGRA�`�����l����ABGR�ɕ��ёւ��� (���]�̈��)
  // ���̃`�����l����: B=0, G=1, R=2, A=3
  // �o�͂̃`�����l����: A=0, R=1, G=2, B=3 (Halide Func�̏o�̓`�����l��c)
  output_func(x, y, c) = select(c == 0, input_func(x, y, 3), // �o��c=0 (A) <-- ����c=3 (A)
   c == 1, input_func(x, y, 2), // �o��c=1 (R) <-- ����c=2 (R)
   c == 2, input_func(x, y, 1), // �o��c=2 (G) <-- ����c=1 (G)
   input_func(x, y, 0)); // �o��c=3 (B) <-- ����c=0 (B)

  // 4. �X�P�W���[�����O (CPU�����ɍœK��)
  // CPU�R�A��Y���ɉ����ĕ��񉻂��AX���Ńx�N�g����
  output_func.parallel(y).vectorize(x, 8); // 8�s�N�Z���P�ʂŃx�N�g�����߂��g�p

  // 5. Halide�p�C�v���C�������s���A���ʂ�Halide::Buffer�Ɋi�[
  Buffer<float> output_halide_buffer = output_func.realize({ width, height, channels });

  // 6. ���ʂ�Halide::Buffer��V����OpenCV Mat�ɕϊ�
  // �o�͗p��cv::Mat���m��
  cv::Mat output_cv_mat(height, width, CV_32FC4);

  // Halide::Buffer����cv::Mat�փf�[�^���R�s�[
  for (int j = 0; j < height; ++j) {
   for (int i = 0; i < width; ++i) {
	for (int k = 0; k < channels; ++k) {
	 // Halide::Buffer�̃f�[�^�� output_halide_buffer(x, y, c)
	 // OpenCV Mat�̃f�[�^�� output_cv_mat.at<cv::Vec4f>(y, x)[c]
	 output_cv_mat.at<cv::Vec4f>(j, i)[k] = output_halide_buffer(i, j, k);
	}
   }
  }

  return output_cv_mat;
 }

 LIBRARY_DLL_API cv::Mat process_bgra_mat_with_halide2(const cv::Mat& input_cv_mat) {
  // 1. ����cv::Mat�̌���
  if (input_cv_mat.empty()) {
   std::cerr << "�G���[: ����cv::Mat����ł��B" << std::endl;
   return cv::Mat(); // ���Mat��Ԃ�
  }
  // BGRA float32 (CV_32FC4) ��z��
  if (input_cv_mat.type() != CV_32FC4) {
   std::cerr << "�G���[: ����cv::Mat�̌^��CV_32FC4�ł͂���܂���B���݂̌^: "
	<< input_cv_mat.type() << std::endl;
   return cv::Mat(); // ���Mat��Ԃ�
  }

  int width = input_cv_mat.cols;
  int height = input_cv_mat.rows;
  // const int channels = input_cv_mat.channels(); // channels �͒萔�ł��邱�Ƃ�ۏ؂��邽�� const ��ǉ����邱�Ƃ𐄏�
  const int channels = input_cv_mat.channels(); // ����͏��4 (B, G, R, A)

  // 2. OpenCV Mat��Halide::Buffer�ɕϊ� (����)
  // input_halide_buffer �� input_cv_mat �̃f�[�^���R�s�[�����Q�Ƃ��܂��B
  Halide::Buffer<float> input_halide_buffer = Halide::Buffer<float>::make_interleaved(
   (float*)input_cv_mat.data,
   width,
   height,
   channels
  );

  // --- �������C�� ---
  // 3. Halide�p�C�v���C���̒�`
  // c �� Func �̈����ɖ߂��܂��B����ɂ�� Func ��3�����ɂȂ�܂��B
  Halide::Var x("x"), y("y"), c("c"); // c �� Var �Ƃ��Ē�`

  // �o��Func: �������ʂ��`
  Halide::Func output_func("output_func");

  // output_func ��3���� (x, y, c) �� Func �Ƃ��Ē�`
  // select �����g���� c �̒l�ɉ����ĈقȂ���̓`�����l�����Q�Ƃ��܂��B
  output_func(x, y, c) = select(c == 0, 1.0f - input_halide_buffer(x, y, 0), // B�`�����l���̔��]
   c == 1, 1.0f - input_halide_buffer(x, y, 1), // G�`�����l���̔��]
   c == 2, 1.0f - input_halide_buffer(x, y, 2), // R�`�����l���̔��]
   input_halide_buffer(x, y, 3));              // A�`�����l���͂��̂܂�
  // --- �C���I��� ---

  // 4. �X�P�W���[�����O
  // ������CPU��GPU�̂ǂ���Ŏ��s���邩��I�����܂��B
  // ���ϐ� HL_TARGET ���ݒ肳��Ă��Ȃ��ꍇ�̃f�t�H���g�� CPU �ł��B

  Halide::Target target = get_target_from_environment();

  // CPU���s�ɏW�����邽�߁AGPU�p�X�͈��������R�����g�A�E�g���܂��B
  // Halide��has_gpu_feature()�����p�ł��Ȃ��Â��o�[�W������z�肵�A
  // GPU�p�X�͈�U�R�����g�A�E�g�܂��͍폜���܂��B
  // �����I��CPU�^�[�Q�b�g�Ƃ��Ĉ����܂��B
  // if (!target.has_gpu_feature()) {
  // target = Target("host-cuda");
  // }

  std::cerr << "Using Halide target: " << target.to_string() << std::endl;

  // CPU �����̃X�P�W���[�����O (�f�t�H���g)
  // Func��3�����ɖ߂����̂ŁAunroll(c)���ēx�K�p�ł��܂��B
  // unroll(c) �́Ac�������萔 (4) �ł��邽�ߗL���ł��B
  output_func.parallel(y); // x������8�v�fSIMD�Ay�����͕���Ac�����̓A�����[��

  output_func.compute_root(); // �p�C�v���C���S�̂����[�g�Ōv�Z

  // 5. Halide�p�C�v���C�������s���A���ʂ�V����OpenCV Mat�ɒ��ڊi�[
  cv::Mat output_cv_mat(height, width, CV_32FC4);

  // output_cv_mat�̃�������Halide::Buffer�Ń��b�v���� (make_interleaved ���Ȍ�)
  // ���� output_halide_buffer ��3���� (x, y, c) �ŁAFunc�̒�`�ƈ�v���܂��B
  Halide::Buffer<float> output_halide_buffer = Halide::Buffer<float>::make_interleaved(
   (float*)output_cv_mat.data,
   width,
   height,
   channels
  );
  std::cerr << "--- Debugging output_halide_buffer properties ---" << std::endl;
  std::cerr << "Buffer dimensions: " << output_halide_buffer.dimensions() << std::endl;
  std::cerr << "Buffer width (extent[0]): " << output_halide_buffer.extent(0) << std::endl;
  std::cerr << "Buffer height (extent[1]): " << output_halide_buffer.extent(1) << std::endl;
  std::cerr << "Buffer channels (extent[2]): " << output_halide_buffer.extent(2) << std::endl;
  std::cerr << "Buffer stride for x (stride[0]): " << output_halide_buffer.stride(0) << std::endl;
  std::cerr << "Buffer stride for y (stride[1]): " << output_halide_buffer.stride(1) << std::endl;
  std::cerr << "Buffer stride for c (stride[2]): " << output_halide_buffer.stride(2) << std::endl;
  std::cerr << "------------------------------------------------" << std::endl;

  // Halide�p�C�v���C���̎��s
  try {
   // Halide�p�C�v���C���̎��s
   // Func��3�����ABuffer��3�����Ȃ̂ŁARealize�̏o�͐�����v���܂��B
   output_func.realize(output_halide_buffer, target);
  }
  catch (const Halide::CompileError& e) {
   std::cerr << "Halide CompileError caught: " << e.what() << std::endl;
   return cv::Mat(); // �G���[���͋��Mat��Ԃ�
  }
  catch (const Halide::RuntimeError& e) {
   std::cerr << "Halide RuntimeError caught: " << e.what() << std::endl;
   return cv::Mat(); // �G���[���͋��Mat��Ԃ�
  }
  catch (const std::exception& e) {
   std::cerr << "Standard C++ exception caught: " << e.what() << std::endl;
   return cv::Mat(); // �G���[���͋��Mat��Ԃ�
  }
  catch (...) {
   std::cerr << "Unknown exception caught." << std::endl;
   return cv::Mat(); // �G���[���͋��Mat��Ԃ�
  }

  return output_cv_mat;
 }



 LIBRARY_DLL_API cv::Mat process_bgra_mat_with_halide_gpu(const cv::Mat& input_cv_mat) {
  // ... (�ȗ�: ���͌��؁AHalide::Buffer�ϊ��ȂǁA�ύX�Ȃ��̕���) ...

  int width = input_cv_mat.cols;
  int height = input_cv_mat.rows;
  const int channels = input_cv_mat.channels();

  Halide::Buffer<float> input_halide_buffer = Halide::Buffer<float>::make_interleaved(
   (float*)input_cv_mat.data,
   width,
   height,
   channels
  );

  std::cerr << "--- Halide Input Buffer Raw Data Debugging ---" << std::endl;
  if (width > 0 && height > 0) {
   // �擪�̐��s�N�Z�����m�F
   for (int p_y = 0; p_y < std::min(height, 5); ++p_y) {
	for (int p_x = 0; p_x < std::min(width, 5); ++p_x) {
	 std::cerr << "Input Halide Buffer (" << p_x << "," << p_y << "): ";
	 for (int p_c = 0; p_c < channels; ++p_c) {
	  // Halide::Buffer �̃A�N�Z�X�� (x, y, c) �̏�
	  std::cerr << input_halide_buffer(p_x, p_y, p_c) << " ";
	 }
	 std::cerr << std::endl;
	}
   }
  }
  std::cerr << "--------------------------------------------" << std::endl;
  // --- �f�o�b�O�o�͂����܂� ---


  // 3. Halide�p�C�v���C���̒�`
  Halide::Var x("x"), y("y"), c("c");
  Halide::Var xi("xi"), yi("yi"), co("co"), ci("ci");

  Halide::Func output_func("output_func");

  // --- �������C�� ---
  // �A���t�@�`�����l�� (c == 3) �͏�� 1.0f (�s����) �ɐݒ�
  output_func(x, y, c) = select(c == 0, 1.0f - input_halide_buffer(x, y, 0), // B�`�����l���̔��]
   c == 1, 1.0f - input_halide_buffer(x, y, 1), // G�`�����l���̔��]
   c == 2, 1.0f - input_halide_buffer(x, y, 2), // R�`�����l���̔��]
   input_halide_buffer(x, y, 3));                                      // A�`�����l���͏��1.0f�ɌŒ�
  // --- �C���I��� ---

  // ... (�ȗ�: Target�ݒ�AGPU�X�P�W���[�����O�Arealize�A�R�s�[�ȂǁA�ύX�Ȃ��̕���) ...

  Halide::Target target = get_target_from_environment();
  if (!target.has_feature(Halide::Target::CUDA)) {
   std::cerr << "Warning: CUDA feature not in target, adding CUDA to target." << std::endl;
   target = target.with_feature(Halide::Target::CUDA);
  }
  std::cerr << "Using Halide target for GPU: " << target.to_string() << std::endl;

  output_func.gpu_tile(x, y, c, xi, yi, ci, 32, 32, channels);
  output_func.unroll(ci);
  output_func.compute_root();

  Halide::Buffer<float> output_halide_gpu_buffer(width, height, channels);
  std::cerr << "--- Debugging output_halide_gpu_buffer properties (before realize) ---" << std::endl;
  std::cerr << "Buffer dimensions: " << output_halide_gpu_buffer.dimensions() << std::endl;
  std::cerr << "Buffer width (extent[0]): " << output_halide_gpu_buffer.extent(0) << std::endl;
  std::cerr << "Buffer height (extent[1]): " << output_halide_gpu_buffer.extent(1) << std::endl;
  std::cerr << "Buffer channels (extent[2]): " << output_halide_gpu_buffer.extent(2) << std::endl;
  std::cerr << "Buffer stride for x (stride[0]): " << output_halide_gpu_buffer.stride(0) << std::endl;
  std::cerr << "Buffer stride for y (stride[1]): " << output_halide_gpu_buffer.stride(1) << std::endl;
  std::cerr << "Buffer stride for c (stride[2]): " << output_halide_gpu_buffer.stride(2) << std::endl;
  std::cerr << "------------------------------------------------------------------" << std::endl;

  try {
   output_func.realize(output_halide_gpu_buffer, target);
   output_halide_gpu_buffer.device_sync();
   output_halide_gpu_buffer.copy_to_host();




   cv::Mat output_cv_mat(height, width, CV_32FC4);
   for (int y_ = 0; y_ < height; ++y_) {
	for (int x_ = 0; x_ < width; ++x_) {
	 for (int c_ = 0; c_ < channels; ++c_) {
	  output_cv_mat.at<cv::Vec4f>(y_, x_)[c_] = output_halide_gpu_buffer(x_, y_, c_);
	 }
	}
   }
   return output_cv_mat;

  }
  catch (const Halide::CompileError& e) {
   std::cerr << "Halide CompileError caught: " << e.what() << std::endl;
   return cv::Mat();
  }
  catch (const Halide::RuntimeError& e) {
   std::cerr << "Halide RuntimeError caught: " << e.what() << std::endl;
   return cv::Mat();
  }
  catch (const std::exception& e) {
   std::cerr << "Standard C++ exception caught: " << e.what() << std::endl;
   return cv::Mat();
  }
  catch (...) {
   std::cerr << "Unknown exception caught." << std::endl;
   return cv::Mat();
  }
 }


};