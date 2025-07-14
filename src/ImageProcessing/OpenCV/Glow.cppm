module;
#include <opencv2/opencv.hpp>
#include <random>
#include "../../../include/Define/DllExportMacro.hpp"


module Glow;

import Image;

namespace ArtifactCore {

 LIBRARY_DLL_API void applySimpleGlow(
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

  // マスク処理
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

   // 色調補正（glowColor）
   cv::Mat colored = blurred.clone();
   for (int y = 0; y < colored.rows; ++y) {
	cv::Vec3f* row = colored.ptr<cv::Vec3f>(y);
	for (int x = 0; x < colored.cols; ++x) {
	 row[x][0] *= glowColor[0] / 255.0f;
	 row[x][1] *= glowColor[1] / 255.0f;
	 row[x][2] *= glowColor[2] / 255.0f;
	}
   }

   // マスク適用
   if (!maskGray.empty()) {
	for (int y = 0; y < colored.rows; ++y) {
	 cv::Vec3f* row = colored.ptr<cv::Vec3f>(y);
	 const float* mrow = maskGray.ptr<float>(y);
	 for (int x = 0; x < colored.cols; ++x) {
	  row[x] *= mrow[x];
	 }
	}
   }

   // アルファ合成
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

  // 1. RGBAからRGBとアルファチャンネルを分離
  // OCV 4.x以降ではsplit()が効率的
  std::vector<cv::Mat> channels;
  cv::split(src, channels);
  rgb_src = channels[0]; // B
  cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgb_src); // BGRとして結合

  alpha_src = channels[3]; // アルファチャンネル

  // 2. 輝度の抽出 (RGB画像をグレースケールに変換し、しきい値処理)
  // 輝度計算: L = 0.2126 * R + 0.7152 * G + 0.0722 * B (Rec.709)
  // OpenCVのcvtColor(BGR2GRAY)も輝度計算を使いますが、floatなので注意
  cv::Mat luminance_float;
  // BGRを直接グレースケールに変換 (float型を維持)
  cv::cvtColor(rgb_src, luminance_float, cv::COLOR_BGR2GRAY);

  // しきい値処理 (指定された閾値より明るい部分を抽出)
  // THRESH_BINARY は閾値より小さい値を0に、大きい値をmaxval(ここでは1.0)にする
  cv::threshold(luminance_float, bright_areas, threshold_value, 1.0, cv::THRESH_BINARY);

  // bright_areasをRGBチャンネルに複製して、元の画像の明るい部分だけを抽出
  std::vector<cv::Mat> bright_channels(3);
  bright_channels[0] = bright_areas.mul(channels[0]); // B
  bright_channels[1] = bright_areas.mul(channels[1]); // G
  bright_channels[2] = bright_areas.mul(channels[2]); // R
  cv::Mat bright_rgb_extract;
  cv::merge(bright_channels, bright_rgb_extract);


  // 3. ぼかし (ガウスぼかしを適用)
  // カーネルサイズは奇数である必要がある
  int kernel_size = blur_radius * 2 + 1;
  if (kernel_size < 1) kernel_size = 1; // 最小1

  cv::GaussianBlur(bright_rgb_extract, glow_map, cv::Size(kernel_size, kernel_size), 0);

  // 4. 合成 (元の画像にぼかした画像を加算)
  // AddWeighted はアルファブレンドだが、ここでは直接加算として使う
  // output = src1 * alpha + src2 * beta + gamma;
  // glow_map * intensity + src
  cv::Mat output_rgb;
  cv::addWeighted(rgb_src, 1.0, glow_map, intensity, 0.0, output_rgb);

  // 最終的な出力画像をBGRAで再結合
  std::vector<cv::Mat> output_channels(4);
  cv::split(output_rgb, output_channels); // BGRをB,G,Rに分離
  output_channels[3] = alpha_src; // 元のアルファチャンネルを再利用

  cv::Mat final_output;
  cv::merge(output_channels, final_output);

  return final_output;
 }

 LIBRARY_DLL_API void drawRandomDottedVerticalLine(cv::Mat& image, int x_center, int line_width, long num_points_to_try, unsigned int seed, const cv::Scalar& point_color, int point_size)
 {
  if (image.empty()) return;

  int width = image.cols;
  int height = image.rows;

  // 1. 縦ラインの輝度マップ（確率マップ）を生成
  cv::Mat prob_map = cv::Mat::zeros(height, width, CV_32FC1); // float型グレースケール

  // ラインの中心から離れるほど確率が低くなるようにグラデーションを作成
  for (int x = 0; x < width; ++x) {
   float dist_from_center = std::abs(x - x_center);
   if (dist_from_center < line_width / 2.0) {
	// ラインの中心に近いほど確率を高くする
	// ガウス関数や線形減衰などで形状を調整
	prob_map.col(x).setTo(1.0f - (dist_from_center / (line_width / 2.0)) * 0.8f); // 線形減衰の例
   }
   else {
	prob_map.col(x).setTo(0.0f); // ラインの範囲外は0
   }
  }
  // 少しぼかして滑らかにする (オプション)
  cv::GaussianBlur(prob_map, prob_map, cv::Size(3, 3), 0);
  // 確率を0.0-1.0に正規化 (setToで最大1.0になるように設定していれば不要な場合も)
  double minVal, maxVal;
  cv::minMaxLoc(prob_map, &minVal, &maxVal);
  if (maxVal > 0) {
   prob_map /= maxVal;
  }


  // 2. 再現性のある乱数ジェネレータの初期化
  std::mt19937 gen(seed);
  std::uniform_real_distribution<> dis(0.0, 1.0);

  // 3. 点生成と描画
  long points_drawn = 0;
  for (long i = 0; i < num_points_to_try; ++i) {
   // ランダムな座標を選択
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
  cv::Mat processed_src = src; // まずは入力画像をそのまま使用

  // 入力画像がCV_32FC4でない場合、変換を試みる
  if (src.empty() || src.channels() != 4 || src.depth() != CV_32F) {
   std::cerr << "Input image is not CV_32FC4. Attempting conversion..." << std::endl;
   processed_src = convertToFloat32RGBA(src); // ヘルパー関数で変換
   if (processed_src.empty()) {
	// 変換に失敗した場合
	std::cerr << "Failed to convert input image to CV_32FC4. Aborting." << std::endl;
	return cv::Mat();
   }
  }

  // ここから、以降の処理では `processed_src` を使用する
  // `processed_src` は常に CV_32FC4 であることが保証される
  cv::Mat glow_map;
  cv::Mat bright_areas;
  cv::Mat rgb_src;   // これはBGR順になる
  cv::Mat alpha_src;

  // 1. BGRAからBGRとアルファチャンネルを分離
  std::vector<cv::Mat> channels;
  cv::split(processed_src, channels); // processed_src が BGRA (B, G, R, A) の順であれば
  // channels[0]=B, channels[1]=G, channels[2]=R, channels[3]=A

  // BGRとして結合（channels[0], channels[1], channels[2] は B, G, R）
  cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgb_src); // これで rgb_src は BGR順になる
  alpha_src = channels[3]; // アルファチャンネル

  // 2. 輝度の抽出 (BGR画像をグレースケールに変換し、しきい値処理)
  cv::Mat luminance_float;
  // ここは元々 COLOR_BGR2GRAY なので修正不要 (rgb_src は BGR順)
  cv::cvtColor(rgb_src, luminance_float, cv::COLOR_BGR2GRAY);

  cv::threshold(luminance_float, bright_areas, threshold_value, 1.0, cv::THRESH_BINARY);

  std::vector<cv::Mat> bright_channels(3);
  // bright_areas は1チャンネル、channels[0] (B), channels[1] (G), channels[2] (R) もそれぞれ1チャンネル
  bright_channels[0] = bright_areas.mul(channels[0]); // Bチャンネルに適用
  bright_channels[1] = bright_areas.mul(channels[1]); // Gチャンネルに適用
  bright_channels[2] = bright_areas.mul(channels[2]); // Rチャンネルに適用
  cv::Mat bright_rgb_extract;
  cv::merge(bright_channels, bright_rgb_extract); // bright_rgb_extract も BGR順

  // 3. 縦方向のみのぼかし (Gaussian Blur with asymmetric kernel)
  int kernel_size_y = vertical_blur_radius * 2 + 1;
  if (kernel_size_y < 1) kernel_size_y = 1;

  int kernel_size_x = vertical_blur_radius * 2 + 1; // 縦と同じロジックで計算
  if (kernel_size_x < 1) kernel_size_x = 1;

  cv::Mat glow_map_vertical;
  cv::Mat glow_map_horizontal;

  cv::GaussianBlur(bright_rgb_extract, glow_map_vertical, cv::Size(1, kernel_size_y), 0); // bright_rgb_extract は BGR順

  cv::GaussianBlur(bright_rgb_extract, glow_map_horizontal, cv::Size(kernel_size_x, 1), 0);

  cv::addWeighted(glow_map_vertical, 1.0, glow_map_horizontal, 1.0, 0.0, glow_map); // 単純に加算

  // 4. 合成 (元の画像にぼかした画像を加算)
  cv::Mat output_rgb;
  cv::addWeighted(rgb_src, 1.0,glow_map, intensity, 0.0, output_rgb); // rgb_src は BGR順、glow_map も BGR順



  // 最終的な出力画像をBGRAで再結合
  std::vector<cv::Mat> output_channels_to_merge; // 新しいベクトルを定義

  // output_rgb は BGR順の3チャンネル画像なので、それを分割
  std::vector<cv::Mat> bgr_channels_from_output;
  cv::split(output_rgb, bgr_channels_from_output); // output_rgbをBGRの3チャンネルに分割

  // 分割したB, G, Rチャンネルを新しいベクトルに追加
  output_channels_to_merge.push_back(bgr_channels_from_output[0]); // Bチャンネル
  output_channels_to_merge.push_back(bgr_channels_from_output[1]); // Gチャンネル
  output_channels_to_merge.push_back(bgr_channels_from_output[2]); // Rチャンネル

  // 最後にアルファチャンネルを追加
  output_channels_to_merge.push_back(alpha_src); // 元のアルファチャンネルを再利用

  cv::Mat final_output;
  // output_channels_to_merge を使ってBGRAとして結合
  cv::merge(output_channels_to_merge, final_output);

  return final_output;
 }

}