module;
#include <opencv2/opencv.hpp>
export module ImageProcessing:NegateCV;

export namespace ArtifactCore
{
 cv::Mat negateBgraFloat32Image(const cv::Mat& input_image) {
  // 入力画像のチェック
  if (input_image.empty()) {
   std::cerr << "Error: Input image is empty." << std::endl;
   return cv::Mat();
  }

  // 画像をBGRA (4チャンネル) に変換
  // imreadはデフォルトでBGRで読み込むため、RGBA/BGRA画像はIMREAD_UNCHANGEDで読み込む
  // しかし、入力が確実にBGRAである保証はないため、変換処理を挟む
  cv::Mat bgra_image;
  if (input_image.channels() == 3) {
   // BGRからBGRAへ変換 (アルファチャンネルを255で追加)
   cv::cvtColor(input_image, bgra_image, cv::COLOR_BGR2BGRA);
  }
  else if (input_image.channels() == 4) {
   // すでに4チャンネルであればそのまま
   input_image.copyTo(bgra_image);
  }
  else {
   std::cerr << "Error: Unsupported number of channels: " << input_image.channels() << std::endl;
   return cv::Mat();
  }

  // float32型 (0.0 - 1.0) に変換
  cv::Mat bgra_float;
  bgra_image.convertTo(bgra_float, CV_32FC4, 1.0 / 255.0);

  // ネガポジ反転処理
  // 各チャンネル (B, G, R) に対して 1.0 - p を計算
  // アルファチャンネルはそのまま (ここではインデックス3)
  cv::Mat negated_bgra_float = bgra_float.clone(); // クローンして元の画像を保持

  // 画像の要素にアクセスして反転
  // ピクセルごとの操作なので、より効率的なOpenCVの関数を利用
  // cv::split でチャンネルを分割し、B, G, R チャンネルのみ反転
  std::vector<cv::Mat> channels;
  cv::split(negated_bgra_float, channels);

  // B, G, R チャンネルを反転 (channels[0], channels[1], channels[2])
  // 1.0 - channel_mat の計算
  channels[0] = 1.0 - channels[0]; // Blue
  channels[1] = 1.0 - channels[1]; // Green
  channels[2] = 1.0 - channels[2]; // Red
  // channels[3] (Alpha) はそのまま

  cv::merge(channels, negated_bgra_float);

  return negated_bgra_float;
 }










};