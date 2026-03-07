
module ;
#include <Halide.h>
#include <opencv2/opencv.hpp>
#include "../../Define/DllExportMacro.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

export module HalideTest;





export namespace ArtifactCore
{
 namespace Internal {
  inline void halide_no_op_free(void* /*user_context*/, void* /*ptr*/) {
   // No-op free function
  }
 }



 using namespace Halide;
 LIBRARY_DLL_API cv::Mat process_bgra_mat_with_halide(const cv::Mat& input_cv_mat) {
  // 1. cv::Mať
  if (input_cv_mat.empty()) {
   std::cerr << "G[: cv::MatłB" << std::endl;
   return cv::Mat(); // MatԂ
  }
  if (input_cv_mat.type() != CV_32FC4) {
   std::cerr << "G[: cv::Mať^CV_32FC4ł͂܂B݂̌^: "
	<< input_cv_mat.type() << std::endl;
   return cv::Mat(); // MatԂ
  }

  int width = input_cv_mat.cols;
  int height = input_cv_mat.rows;
  int channels = input_cv_mat.channels(); // ͏4

  // 2. OpenCV MatHalide::Bufferɕϊ
  // make_interleaved̓C^[[uꂽ`lf[^̂ɓKĂ܂B
  // (x, y, c) ̏ŃANZXł悤ɂȂ܂B
  Buffer<float> input_halide_buffer = Buffer<float>::make_interleaved(
   (float*)input_cv_mat.data, // Mat̐f[^|C^
   width,                     // 
   height,                    // 
   channels                   // `l (4)
  );

  // 3. HalidepCvC̒`
  Var x("x"), y("y"), c("c"); // Wƃ`l̕ϐ

  // Func: input_halide_bufferFuncƂăbv
  Func input_func("input_func");
  input_func(x, y, c) = input_halide_buffer(x, y, c);

  // oFunc: ʂ`
  Func output_func("output_func");

  // ȒPȏ: BGRA`lABGRɕёւ (]̈)
  // ̃`l: B=0, G=1, R=2, A=3
  // o͂̃`l: A=0, R=1, G=2, B=3 (Halide Func̏o̓`lc)
  output_func(x, y, c) = select(c == 0, input_func(x, y, 3), // oc=0 (A) <-- c=3 (A)
   c == 1, input_func(x, y, 2), // oc=1 (R) <-- c=2 (R)
   c == 2, input_func(x, y, 1), // oc=2 (G) <-- c=1 (G)
   input_func(x, y, 0)); // oc=3 (B) <-- c=0 (B)

  // 4. XPW[O (CPUɍœK)
  // CPURAYɉĕ񉻂AXŃxNg
  output_func.parallel(y).vectorize(x, 8); // 8sNZPʂŃxNg߂gp

  // 5. HalidepCvCsAʂHalide::BufferɊi[
  Buffer<float> output_halide_buffer = output_func.realize({ width, height, channels });

  // 6. ʂHalide::BufferVOpenCV Matɕϊ
  // o͗pcv::Matm
  cv::Mat output_cv_mat(height, width, CV_32FC4);

  // Halide::Buffercv::Matփf[^Rs[
  for (int j = 0; j < height; ++j) {
   for (int i = 0; i < width; ++i) {
	for (int k = 0; k < channels; ++k) {
	 // Halide::Buffer̃f[^ output_halide_buffer(x, y, c)
	 // OpenCV Mat̃f[^ output_cv_mat.at<cv::Vec4f>(y, x)[c]
	 output_cv_mat.at<cv::Vec4f>(j, i)[k] = output_halide_buffer(i, j, k);
	}
   }
  }

  return output_cv_mat;
 }

 LIBRARY_DLL_API cv::Mat process_bgra_mat_with_halide2(const cv::Mat& input_cv_mat) {
  // 1. cv::Mať
  if (input_cv_mat.empty()) {
   std::cerr << "G[: cv::MatłB" << std::endl;
   return cv::Mat(); // MatԂ
  }
  // BGRA float32 (CV_32FC4) z
  if (input_cv_mat.type() != CV_32FC4) {
   std::cerr << "G[: cv::Mať^CV_32FC4ł͂܂B݂̌^: "
	<< input_cv_mat.type() << std::endl;
   return cv::Mat(); // MatԂ
  }

  int width = input_cv_mat.cols;
  int height = input_cv_mat.rows;
  // const int channels = input_cv_mat.channels(); // channels ͒萔ł邱Ƃۏ؂邽 const ǉ邱Ƃ𐄏
  const int channels = input_cv_mat.channels(); // ͏4 (B, G, R, A)

  // 2. OpenCV MatHalide::Bufferɕϊ ()
  // input_halide_buffer  input_cv_mat ̃f[^Rs[QƂ܂B
  Halide::Buffer<float> input_halide_buffer = Halide::Buffer<float>::make_interleaved(
   (float*)input_cv_mat.data,
   width,
   height,
   channels
  );

  // --- C ---
  // 3. HalidepCvC̒`
  // c  Func ̈ɖ߂܂Bɂ Func 3ɂȂ܂B
  Halide::Var x("x"), y("y"), c("c"); // c  Var ƂĒ`

  // oFunc: ʂ`
  Halide::Func output_func("output_func");

  // output_func 3 (x, y, c)  Func ƂĒ`
  // select g c ̒lɉĈقȂ̓`lQƂ܂B
  output_func(x, y, c) = select(c == 0, 1.0f - input_halide_buffer(x, y, 0), // B`l̔]
   c == 1, 1.0f - input_halide_buffer(x, y, 1), // G`l̔]
   c == 2, 1.0f - input_halide_buffer(x, y, 2), // R`l̔]
   input_halide_buffer(x, y, 3));              // A`l͂̂܂
  // --- CI ---

  // 4. XPW[O
  // CPUGPÛǂŎs邩I܂B
  // ϐ HL_TARGET ݒ肳ĂȂꍇ̃ftHg CPU łB

  Halide::Target target = get_target_from_environment();

  // CPUsɏW邽߁AGPUpX͈RgAEg܂B
  // Halidehas_gpu_feature()płȂÂo[Wz肵A
  // GPUpX͈URgAEg܂͍폜܂B
  // ICPU^[QbgƂĈ܂B
  // if (!target.has_gpu_feature()) {
  // target = Target("host-cuda");
  // }

  std::cerr << "Using Halide target: " << target.to_string() << std::endl;

  // CPU ̃XPW[O (ftHg)
  // Func3ɖ߂̂ŁAunroll(c)ēxKpł܂B
  // unroll(c) ́Ac萔 (4) ł邽ߗLłB
  output_func.parallel(y); // x8vfSIMDAy͕Ac̓A[

  output_func.compute_root(); // pCvCŜ[gŌvZ

  // 5. HalidepCvCsAʂVOpenCV Matɒڊi[
  cv::Mat output_cv_mat(height, width, CV_32FC4);

  // output_cv_mat̃Halide::BufferŃbv (make_interleaved Ȍ)
  //  output_halide_buffer 3 (x, y, c) ŁAFunc̒`ƈv܂B
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

  // HalidepCvC̎s
  try {
   // HalidepCvC̎s
   // Func3ABuffer3Ȃ̂ŁARealizȅo͐v܂B
   output_func.realize(output_halide_buffer, target);
  }
  catch (const Halide::CompileError& e) {
   std::cerr << "Halide CompileError caught: " << e.what() << std::endl;
   return cv::Mat(); // G[͋MatԂ
  }
  catch (const Halide::RuntimeError& e) {
   std::cerr << "Halide RuntimeError caught: " << e.what() << std::endl;
   return cv::Mat(); // G[͋MatԂ
  }
  catch (const std::exception& e) {
   std::cerr << "Standard C++ exception caught: " << e.what() << std::endl;
   return cv::Mat(); // G[͋MatԂ
  }
  catch (...) {
   std::cerr << "Unknown exception caught." << std::endl;
   return cv::Mat(); // G[͋MatԂ
  }

  return output_cv_mat;
 }



 LIBRARY_DLL_API cv::Mat process_bgra_mat_with_halide_gpu(const cv::Mat& input_cv_mat) {
  // ... (ȗ: ͌؁AHalide::BufferϊȂǁAύXȂ̕) ...

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
   // 擪̐sNZmF
   for (int p_y = 0; p_y < std::min(height, 5); ++p_y) {
	for (int p_x = 0; p_x < std::min(width, 5); ++p_x) {
	 std::cerr << "Input Halide Buffer (" << p_x << "," << p_y << "): ";
	 for (int p_c = 0; p_c < channels; ++p_c) {
	  // Halide::Buffer ̃ANZX (x, y, c) ̏
	  std::cerr << input_halide_buffer(p_x, p_y, p_c) << " ";
	 }
	 std::cerr << std::endl;
	}
   }
  }
  std::cerr << "--------------------------------------------" << std::endl;
  // --- fobOo͂܂ ---


  // 3. HalidepCvC̒`
  Halide::Var x("x"), y("y"), c("c");
  Halide::Var xi("xi"), yi("yi"), co("co"), ci("ci");

  Halide::Func output_func("output_func");

  // --- C ---
  // At@`l (c == 3) ͏ 1.0f (s) ɐݒ
  output_func(x, y, c) = select(c == 0, 1.0f - input_halide_buffer(x, y, 0), // B`l̔]
   c == 1, 1.0f - input_halide_buffer(x, y, 1), // G`l̔]
   c == 2, 1.0f - input_halide_buffer(x, y, 2), // R`l̔]
   input_halide_buffer(x, y, 3));                                      // A`l͏1.0fɌŒ
  // --- CI ---

  // ... (ȗ: TargetݒAGPUXPW[OArealizeARs[ȂǁAύXȂ̕) ...

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