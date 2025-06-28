
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
  // 1. 入力cv::Matの検証
  if (input_cv_mat.empty()) {
   std::cerr << "エラー: 入力cv::Matが空です。" << std::endl;
   return cv::Mat(); // 空のMatを返す
  }
  if (input_cv_mat.type() != CV_32FC4) {
   std::cerr << "エラー: 入力cv::Matの型がCV_32FC4ではありません。現在の型: "
	<< input_cv_mat.type() << std::endl;
   return cv::Mat(); // 空のMatを返す
  }

  int width = input_cv_mat.cols;
  int height = input_cv_mat.rows;
  int channels = input_cv_mat.channels(); // これは常に4

  // 2. OpenCV MatをHalide::Bufferに変換
  // make_interleavedはインターリーブされたチャンネルデータを扱うのに適しています。
  // (x, y, c) の順でアクセスできるようになります。
  Buffer<float> input_halide_buffer = Buffer<float>::make_interleaved(
   (float*)input_cv_mat.data, // Matの生データポインタ
   width,                     // 幅
   height,                    // 高さ
   channels                   // チャンネル数 (4)
  );

  // 3. Halideパイプラインの定義
  Var x("x"), y("y"), c("c"); // 座標とチャンネルの変数

  // 入力Func: input_halide_bufferをFuncとしてラップ
  Func input_func("input_func");
  input_func(x, y, c) = input_halide_buffer(x, y, c);

  // 出力Func: 処理結果を定義
  Func output_func("output_func");

  // 簡単な処理: BGRAチャンネルをABGRに並び替える (反転の一種)
  // 元のチャンネル順: B=0, G=1, R=2, A=3
  // 出力のチャンネル順: A=0, R=1, G=2, B=3 (Halide Funcの出力チャンネルc)
  output_func(x, y, c) = select(c == 0, input_func(x, y, 3), // 出力c=0 (A) <-- 入力c=3 (A)
   c == 1, input_func(x, y, 2), // 出力c=1 (R) <-- 入力c=2 (R)
   c == 2, input_func(x, y, 1), // 出力c=2 (G) <-- 入力c=1 (G)
   input_func(x, y, 0)); // 出力c=3 (B) <-- 入力c=0 (B)

  // 4. スケジューリング (CPU向けに最適化)
  // CPUコアでY軸に沿って並列化し、X軸でベクトル化
  output_func.parallel(y).vectorize(x, 8); // 8ピクセル単位でベクトル命令を使用

  // 5. Halideパイプラインを実行し、結果をHalide::Bufferに格納
  Buffer<float> output_halide_buffer = output_func.realize({ width, height, channels });

  // 6. 結果のHalide::Bufferを新しいOpenCV Matに変換
  // 出力用のcv::Matを確保
  cv::Mat output_cv_mat(height, width, CV_32FC4);

  // Halide::Bufferからcv::Matへデータをコピー
  for (int j = 0; j < height; ++j) {
   for (int i = 0; i < width; ++i) {
	for (int k = 0; k < channels; ++k) {
	 // Halide::Bufferのデータは output_halide_buffer(x, y, c)
	 // OpenCV Matのデータは output_cv_mat.at<cv::Vec4f>(y, x)[c]
	 output_cv_mat.at<cv::Vec4f>(j, i)[k] = output_halide_buffer(i, j, k);
	}
   }
  }

  return output_cv_mat;
 }

 LIBRARY_DLL_API cv::Mat process_bgra_mat_with_halide2(const cv::Mat& input_cv_mat) {
  // 1. 入力cv::Matの検証
  if (input_cv_mat.empty()) {
   std::cerr << "エラー: 入力cv::Matが空です。" << std::endl;
   return cv::Mat(); // 空のMatを返す
  }
  // BGRA float32 (CV_32FC4) を想定
  if (input_cv_mat.type() != CV_32FC4) {
   std::cerr << "エラー: 入力cv::Matの型がCV_32FC4ではありません。現在の型: "
	<< input_cv_mat.type() << std::endl;
   return cv::Mat(); // 空のMatを返す
  }

  int width = input_cv_mat.cols;
  int height = input_cv_mat.rows;
  // const int channels = input_cv_mat.channels(); // channels は定数であることを保証するため const を追加することを推奨
  const int channels = input_cv_mat.channels(); // これは常に4 (B, G, R, A)

  // 2. OpenCV MatをHalide::Bufferに変換 (入力)
  // input_halide_buffer は input_cv_mat のデータをコピーせず参照します。
  Halide::Buffer<float> input_halide_buffer = Halide::Buffer<float>::make_interleaved(
   (float*)input_cv_mat.data,
   width,
   height,
   channels
  );

  // --- ここを修正 ---
  // 3. Halideパイプラインの定義
  // c を Func の引数に戻します。これにより Func は3次元になります。
  Halide::Var x("x"), y("y"), c("c"); // c を Var として定義

  // 出力Func: 処理結果を定義
  Halide::Func output_func("output_func");

  // output_func を3次元 (x, y, c) の Func として定義
  // select 文を使って c の値に応じて異なる入力チャンネルを参照します。
  output_func(x, y, c) = select(c == 0, 1.0f - input_halide_buffer(x, y, 0), // Bチャンネルの反転
   c == 1, 1.0f - input_halide_buffer(x, y, 1), // Gチャンネルの反転
   c == 2, 1.0f - input_halide_buffer(x, y, 2), // Rチャンネルの反転
   input_halide_buffer(x, y, 3));              // Aチャンネルはそのまま
  // --- 修正終わり ---

  // 4. スケジューリング
  // ここでCPUとGPUのどちらで実行するかを選択します。
  // 環境変数 HL_TARGET が設定されていない場合のデフォルトは CPU です。

  Halide::Target target = get_target_from_environment();

  // CPU実行に集中するため、GPUパスは引き続きコメントアウトします。
  // Halideのhas_gpu_feature()が利用できない古いバージョンを想定し、
  // GPUパスは一旦コメントアウトまたは削除します。
  // 強制的にCPUターゲットとして扱います。
  // if (!target.has_gpu_feature()) {
  // target = Target("host-cuda");
  // }

  std::cerr << "Using Halide target: " << target.to_string() << std::endl;

  // CPU 向けのスケジューリング (デフォルト)
  // Funcが3次元に戻ったので、unroll(c)を再度適用できます。
  // unroll(c) は、c次元が定数 (4) であるため有効です。
  output_func.parallel(y); // x方向は8要素SIMD、y方向は並列、c方向はアンロール

  output_func.compute_root(); // パイプライン全体をルートで計算

  // 5. Halideパイプラインを実行し、結果を新しいOpenCV Matに直接格納
  cv::Mat output_cv_mat(height, width, CV_32FC4);

  // output_cv_matのメモリをHalide::Bufferでラップする (make_interleaved が簡潔)
  // この output_halide_buffer は3次元 (x, y, c) で、Funcの定義と一致します。
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

  // Halideパイプラインの実行
  try {
   // Halideパイプラインの実行
   // Funcが3次元、Bufferも3次元なので、Realizeの出力数が一致します。
   output_func.realize(output_halide_buffer, target);
  }
  catch (const Halide::CompileError& e) {
   std::cerr << "Halide CompileError caught: " << e.what() << std::endl;
   return cv::Mat(); // エラー時は空のMatを返す
  }
  catch (const Halide::RuntimeError& e) {
   std::cerr << "Halide RuntimeError caught: " << e.what() << std::endl;
   return cv::Mat(); // エラー時は空のMatを返す
  }
  catch (const std::exception& e) {
   std::cerr << "Standard C++ exception caught: " << e.what() << std::endl;
   return cv::Mat(); // エラー時は空のMatを返す
  }
  catch (...) {
   std::cerr << "Unknown exception caught." << std::endl;
   return cv::Mat(); // エラー時は空のMatを返す
  }

  return output_cv_mat;
 }



 LIBRARY_DLL_API cv::Mat process_bgra_mat_with_halide_gpu(const cv::Mat& input_cv_mat) {
  // ... (省略: 入力検証、Halide::Buffer変換など、変更なしの部分) ...

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
   // 先頭の数ピクセルを確認
   for (int p_y = 0; p_y < std::min(height, 5); ++p_y) {
	for (int p_x = 0; p_x < std::min(width, 5); ++p_x) {
	 std::cerr << "Input Halide Buffer (" << p_x << "," << p_y << "): ";
	 for (int p_c = 0; p_c < channels; ++p_c) {
	  // Halide::Buffer のアクセスは (x, y, c) の順
	  std::cerr << input_halide_buffer(p_x, p_y, p_c) << " ";
	 }
	 std::cerr << std::endl;
	}
   }
  }
  std::cerr << "--------------------------------------------" << std::endl;
  // --- デバッグ出力ここまで ---


  // 3. Halideパイプラインの定義
  Halide::Var x("x"), y("y"), c("c");
  Halide::Var xi("xi"), yi("yi"), co("co"), ci("ci");

  Halide::Func output_func("output_func");

  // --- ここを修正 ---
  // アルファチャンネル (c == 3) は常に 1.0f (不透明) に設定
  output_func(x, y, c) = select(c == 0, 1.0f - input_halide_buffer(x, y, 0), // Bチャンネルの反転
   c == 1, 1.0f - input_halide_buffer(x, y, 1), // Gチャンネルの反転
   c == 2, 1.0f - input_halide_buffer(x, y, 2), // Rチャンネルの反転
   input_halide_buffer(x, y, 3));                                      // Aチャンネルは常に1.0fに固定
  // --- 修正終わり ---

  // ... (省略: Target設定、GPUスケジューリング、realize、コピーなど、変更なしの部分) ...

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