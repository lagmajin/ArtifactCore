module;
#include <opencv2/opencv.hpp>
#include <QDebug>
#include <QElapsedTimer>
module ImageProcessing.SpectralGlow;


import std;

namespace ArtifactCore {


 std::vector<cv::Vec3f> glowColors = {
	{1.0f, 0.0f, 0.0f},   // 赤
	{1.0f, 0.5f, 0.0f},   // オレンジ
	{1.0f, 1.0f, 0.0f},   // 黄
	{0.0f, 1.0f, 0.0f},   // 緑
	{0.0f, 1.0f, 1.0f},   // 水
	{0.0f, 0.0f, 1.0f},   // 青
	{1.0f, 0.0f, 1.0f}    // 紫
 };

 SpectralGlow::SpectralGlow()
 {

 }

 SpectralGlow::~SpectralGlow()
 {

 }

 void SpectralGlow::Process(cv::Mat& mat)
 {
  CV_Assert(mat.type() == CV_32FC4); // BGRA float image

  // === 1. 発光元のマスク抽出（輝度 > 閾値）===
  cv::Mat gray;
  cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY); // 0.0〜1.0

  cv::Mat mask;
  cv::threshold(gray, mask, 0.1f, 1.0f, cv::THRESH_BINARY); // floatマスク

  // === 2. ランダムな色（7色）を定義 ===
  std::vector<cv::Vec3f> glowColors = {
	  {1.0f, 0.0f, 0.0f},   // 赤
	  {1.0f, 0.5f, 0.0f},   // オレンジ
	  {1.0f, 1.0f, 0.0f},   // 黄
	  {0.0f, 1.0f, 0.0f},   // 緑
	  {0.0f, 1.0f, 1.0f},   // 水
	  {0.0f, 0.0f, 1.0f},   // 青
	  {1.0f, 0.0f, 1.0f}    // 紫
  };

  // === 3. ノイズでランダム色インデックス作成 ===
  cv::Mat noise(mask.size(), CV_8U);
  cv::randu(noise, 0, static_cast<int>(glowColors.size()));  // 0〜6

  // === 4. グロー元画像を構築（色 × マスク）===
  cv::Mat glowSrc(mat.size(), CV_32FC3, cv::Scalar(0, 0, 0));
  for (int y = 0; y < mat.rows; ++y)
  {
   const float* mrow = mask.ptr<float>(y);
   const uchar* nrow = noise.ptr<uchar>(y);
   cv::Vec3f* grow = glowSrc.ptr<cv::Vec3f>(y);

   for (int x = 0; x < mat.cols; ++x)
   {
	if (mrow[x] > 0.0f)
	 grow[x] = glowColors[nrow[x]] * mrow[x];
   }
  }

  // === 5. ブラー処理で発光感 ===
  cv::Mat glowBlurred;
  cv::GaussianBlur(glowSrc, glowBlurred, cv::Size(0, 0), 10.0);

  // === 6. グローの強度を調整（ここで輝きの強さを決定） ===
  float glowStrength = 0.4f;
  glowBlurred *= glowStrength;

  // === 7. 元のBGRA画像に加算合成 ===
  for (int y = 0; y < mat.rows; ++y)
  {
   cv::Vec4f* mrow = mat.ptr<cv::Vec4f>(y);
   const cv::Vec3f* grow = glowBlurred.ptr<cv::Vec3f>(y);

   for (int x = 0; x < mat.cols; ++x)
   {
	mrow[x][0] = std::min(mrow[x][0] + grow[x][0], 1.0f); // B
	mrow[x][1] = std::min(mrow[x][1] + grow[x][1], 1.0f); // G
	mrow[x][2] = std::min(mrow[x][2] + grow[x][2], 1.0f); // R
	// A はそのまま保持
   }
  }
 }

 void SpectralGlow::Process2(cv::Mat& mat)
 {
  CV_Assert(mat.type() == CV_32FC4); // BGRA float image

  // === 1. 輝度マスク抽出 ===
  cv::Mat gray;
  cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);
  cv::Mat mask;
  cv::threshold(gray, mask, 0.2f, 1.0f, cv::THRESH_BINARY); // 明るい部分だけ抽出

  // === 2. グロー元（R/G/B）作成 ===
  cv::Mat red(mat.size(), CV_32FC1, cv::Scalar(0));
  cv::Mat green = red.clone();
  cv::Mat blue = red.clone();

  for (int y = 0; y < mat.rows; ++y)
  {
   const float* m = mask.ptr<float>(y);
   float* r = red.ptr<float>(y);
   float* g = green.ptr<float>(y);
   float* b = blue.ptr<float>(y);
   for (int x = 0; x < mat.cols; ++x)
   {
	if (m[x] > 0.0f)
	{
	 // 白発光 → RGB均等（後でズラしてにじみ）
	 float v = m[x];
	 r[x] = v;
	 g[x] = v;
	 b[x] = v;
	}
   }
  }

  // === 3. 位置シフト（プリズムずらし）===
  auto shift = [](const cv::Mat& src, int dx, int dy) -> cv::Mat {
   cv::Mat dst;
   cv::Mat M = (cv::Mat_<float>(2, 3) << 1, 0, dx, 0, 1, dy);
   cv::warpAffine(src, dst, M, src.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, 0);
   return dst;
   };

  cv::Mat r_shifted = shift(red, 1, 0);  // →
  cv::Mat g_shifted = shift(green, 0, 1);  // ↓
  cv::Mat b_shifted = shift(blue, -1, -1);  // ↖

  // === 4. ブラー ===
  cv::GaussianBlur(r_shifted, r_shifted, cv::Size(0, 0), 6.0);
  cv::GaussianBlur(g_shifted, g_shifted, cv::Size(0, 0), 6.0);
  cv::GaussianBlur(b_shifted, b_shifted, cv::Size(0, 0), 6.0);

  // === 5. 合成 ===
  cv::Mat glow(mat.size(), CV_32FC3);
  std::vector<cv::Mat> channels = { b_shifted, g_shifted, r_shifted };
  cv::merge(channels, glow);

  // 強度スケーリング（にじみ感だけ残す）
  glow *= 0.15f;

  // === 6. 元画像に加算合成 ===
  for (int y = 0; y < mat.rows; ++y)
  {
   cv::Vec4f* mrow = mat.ptr<cv::Vec4f>(y);
   const cv::Vec3f* grow = glow.ptr<cv::Vec3f>(y);

   for (int x = 0; x < mat.cols; ++x)
   {
	mrow[x][0] = std::min(mrow[x][0] + grow[x][0], 1.0f); // B
	mrow[x][1] = std::min(mrow[x][1] + grow[x][1], 1.0f); // G
	mrow[x][2] = std::min(mrow[x][2] + grow[x][2], 1.0f); // R
	// Aそのまま
   }
  }
 }

 void SpectralGlow::Process3(cv::Mat& mat)
 {
  CV_Assert(mat.type() == CV_32FC4); // BGRA float

  // 1. 輝度マスク
  cv::Mat gray;
  cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);
  cv::Mat mask;
  cv::threshold(gray, mask, 0.2f, 1.0f, cv::THRESH_BINARY);

  // 2. 各チャネルの輝き原画像作成
  cv::Mat red(mat.size(), CV_32FC1, cv::Scalar(0));
  cv::Mat green = red.clone();
  cv::Mat blue = red.clone();

  for (int y = 0; y < mat.rows; ++y)
  {
   const float* m = mask.ptr<float>(y);
   float* r = red.ptr<float>(y);
   float* g = green.ptr<float>(y);
   float* b = blue.ptr<float>(y);
   for (int x = 0; x < mat.cols; ++x)
   {
	if (m[x] > 0)
	{
	 float v = m[x];
	 r[x] = v;
	 g[x] = v;
	 b[x] = v;
	}
   }
  }

  // 3. チャネルずらし関数
  auto shift = [](const cv::Mat& src, int dx, int dy) -> cv::Mat {
   cv::Mat dst;
   cv::Mat M = (cv::Mat_<float>(2, 3) << 1, 0, dx, 0, 1, dy);
   cv::warpAffine(src, dst, M, src.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, 0);
   return dst;
   };

  cv::Mat r_shifted = shift(red, 1, 0);
  cv::Mat g_shifted = shift(green, 0, 1);
  cv::Mat b_shifted = shift(blue, -1, -1);

  // 4. ブラー
  cv::GaussianBlur(r_shifted, r_shifted, cv::Size(0, 0), 6.0);
  cv::GaussianBlur(g_shifted, g_shifted, cv::Size(0, 0), 6.0);
  cv::GaussianBlur(b_shifted, b_shifted, cv::Size(0, 0), 6.0);

  // 5. 色にじみのためのチャネル混合（にじみ効果UP）
  cv::Mat r_mix = 0.7f * r_shifted + 0.15f * g_shifted + 0.15f * b_shifted;
  cv::Mat g_mix = 0.7f * g_shifted + 0.15f * r_shifted + 0.15f * b_shifted;
  cv::Mat b_mix = 0.7f * b_shifted + 0.15f * g_shifted + 0.15f * r_shifted;

  // 6. 合成
  cv::Mat glow;
  cv::merge(std::vector<cv::Mat>{b_mix, g_mix, r_mix}, glow);

  // 7. 強度調整
  glow *= 0.15f;

  // 8. 元画像に加算（clamp）
  for (int y = 0; y < mat.rows; ++y)
  {
   cv::Vec4f* row = mat.ptr<cv::Vec4f>(y);
   const cv::Vec3f* glow_row = glow.ptr<cv::Vec3f>(y);
   for (int x = 0; x < mat.cols; ++x)
   {
	row[x][0] = std::min(row[x][0] + glow_row[x][0], 1.0f); // B
	row[x][1] = std::min(row[x][1] + glow_row[x][1], 1.0f); // G
	row[x][2] = std::min(row[x][2] + glow_row[x][2], 1.0f); // R
	// alpha unchanged
   }
  }
 }

 void SpectralGlow::Process4(cv::Mat& mat)
 {
  CV_Assert(mat.type() == CV_32FC4); // BGRA float

  // 1. グレースケール化
  cv::Mat gray;
  cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);

  // 2. Sobelで勾配計算
  cv::Mat grad_x, grad_y;
  cv::Sobel(gray, grad_x, CV_32F, 1, 0, 3);
  cv::Sobel(gray, grad_y, CV_32F, 0, 1, 3);

  // 3. 勾配方向計算（ラジアン）
  cv::Mat angle;
  cv::phase(grad_x, grad_y, angle, true); // 角度0-360度

  // 4. 画像全体の平均勾配方向を計算
  double sum_sin = 0, sum_cos = 0;
  int count = 0;
  for (int y = 0; y < angle.rows; ++y)
  {
   const float* a = angle.ptr<float>(y);
   for (int x = 0; x < angle.cols; ++x)
   {
	float deg = a[x];
	float rad = deg * (float)(CV_PI / 180.0);
	sum_cos += std::cos(rad);
	sum_sin += std::sin(rad);
	++count;
   }
  }
  float avg_rad = std::atan2(sum_sin / count, sum_cos / count);

  // にじみ方向は勾配に垂直だから
  float glow_dir_rad = avg_rad + (float)(CV_PI / 2.0);

  // 5. RGBのずらし距離(px)
  int shiftAmount = 2;

  // 6. ずらしベクトル計算
  cv::Point2f shiftR(std::cos(glow_dir_rad), std::sin(glow_dir_rad));
  cv::Point2f shiftG(std::cos(glow_dir_rad + 2.0f), std::sin(glow_dir_rad + 2.0f));
  cv::Point2f shiftB(std::cos(glow_dir_rad - 2.0f), std::sin(glow_dir_rad - 2.0f));

  // 7. 輝度マスク
  cv::Mat mask;
  cv::threshold(gray, mask, 0.2f, 1.0f, cv::THRESH_BINARY);

  // 8. 各チャネル元作成
  cv::Mat red(mat.size(), CV_32FC1, cv::Scalar(0));
  cv::Mat green = red.clone();
  cv::Mat blue = red.clone();

  for (int y = 0; y < mat.rows; ++y)
  {
   const float* m = mask.ptr<float>(y);
   float* r = red.ptr<float>(y);
   float* g = green.ptr<float>(y);
   float* b = blue.ptr<float>(y);
   for (int x = 0; x < mat.cols; ++x)
   {
	if (m[x] > 0)
	{
	 float v = m[x];
	 r[x] = v;
	 g[x] = v;
	 b[x] = v;
	}
   }
  }

  // 9. シフト関数（浮動小数オフセット対応）
  auto shiftFloat = [&](const cv::Mat& src, float dx, float dy) -> cv::Mat {
   cv::Mat dst;
   cv::Mat M = (cv::Mat_<float>(2, 3) << 1, 0, dx, 0, 1, dy);
   cv::warpAffine(src, dst, M, src.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, 0);
   return dst;
   };

  cv::Mat r_shifted = shiftFloat(red, shiftR.x * shiftAmount, shiftR.y * shiftAmount);
  cv::Mat g_shifted = shiftFloat(green, shiftG.x * shiftAmount, shiftG.y * shiftAmount);
  cv::Mat b_shifted = shiftFloat(blue, shiftB.x * shiftAmount, shiftB.y * shiftAmount);

  // 10. ブラー
  cv::GaussianBlur(r_shifted, r_shifted, cv::Size(0, 0), 6.0);
  cv::GaussianBlur(g_shifted, g_shifted, cv::Size(0, 0), 6.0);
  cv::GaussianBlur(b_shifted, b_shifted, cv::Size(0, 0), 6.0);

  // 11. 色にじみミックス
  cv::Mat r_mix = 0.7f * r_shifted + 0.15f * g_shifted + 0.15f * b_shifted;
  cv::Mat g_mix = 0.7f * g_shifted + 0.15f * r_shifted + 0.15f * b_shifted;
  cv::Mat b_mix = 0.7f * b_shifted + 0.15f * g_shifted + 0.15f * r_shifted;

  // 12. 合成
  cv::Mat glow;
  cv::merge(std::vector<cv::Mat>{b_mix, g_mix, r_mix}, glow);

  // 13. 強度調整
  glow *= 0.12f;

  // 14. 加算合成（clamp）
  for (int y = 0; y < mat.rows; ++y)
  {
   cv::Vec4f* row = mat.ptr<cv::Vec4f>(y);
   const cv::Vec3f* glow_row = glow.ptr<cv::Vec3f>(y);
   for (int x = 0; x < mat.cols; ++x)
   {
	row[x][0] = std::min(row[x][0] + glow_row[x][0], 1.0f);
	row[x][1] = std::min(row[x][1] + glow_row[x][1], 1.0f);
	row[x][2] = std::min(row[x][2] + glow_row[x][2], 1.0f);
	// alpha はそのまま
   }
  }
 }

 void SpectralGlow::ElegantGlow(cv::Mat& mat)
 {
  QElapsedTimer timer;
  timer.start();

  CV_Assert(mat.type() == CV_32FC4); // BGRA float

  // 1. グレースケール化
  cv::Mat gray;
  cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);

  // 2. Sobelで勾配計算
  cv::Mat grad_x, grad_y;
  cv::Sobel(gray, grad_x, CV_32F, 1, 0, 3);
  cv::Sobel(gray, grad_y, CV_32F, 0, 1, 3);

  // 3. 勾配方向計算（ラジアン）
  cv::Mat angle;
  cv::phase(grad_x, grad_y, angle, true); // 0-360度

  // 4. 画像全体の平均勾配方向
  double sum_sin = 0, sum_cos = 0;
  int count = 0;
  for (int y = 0; y < angle.rows; ++y)
  {
   const float* a = angle.ptr<float>(y);
   for (int x = 0; x < angle.cols; ++x)
   {
	float deg = a[x];
	float rad = deg * (float)(CV_PI / 180.0);
	sum_cos += std::cos(rad);
	sum_sin += std::sin(rad);
	++count;
   }
  }
  float avg_rad = std::atan2(sum_sin / count, sum_cos / count);
  float glow_dir_rad = avg_rad + (float)(CV_PI / 2.0);

  int shiftAmount = 2;

  auto shiftFloat = [&](const cv::Mat& src, float dx, float dy) -> cv::Mat {
   cv::Mat dst;
   cv::Mat M = (cv::Mat_<float>(2, 3) << 1, 0, dx, 0, 1, dy);
   cv::warpAffine(src, dst, M, src.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, 0);
   return dst;
   };

  cv::Mat mask;
  cv::threshold(gray, mask, 0.2f, 1.0f, cv::THRESH_BINARY);

  cv::Mat red(mat.size(), CV_32FC1, cv::Scalar(0));
  cv::Mat green = red.clone();
  cv::Mat blue = red.clone();

  for (int y = 0; y < mat.rows; ++y)
  {
   const float* m = mask.ptr<float>(y);
   float* r = red.ptr<float>(y);
   float* g = green.ptr<float>(y);
   float* b = blue.ptr<float>(y);
   for (int x = 0; x < mat.cols; ++x)
   {
	if (m[x] > 0)
	{
	 float v = m[x];
	 r[x] = v;
	 g[x] = v;
	 b[x] = v;
	}
   }
  }

  // 5. マルチスケール用のσリスト
  std::vector<double> blurSigmas = { 6.0, 12.0, 24.0 };

  cv::Mat glowTotal = cv::Mat::zeros(mat.size(), CV_32FC3);

  for (double sigma : blurSigmas)
  {
   // シフト距離はσに比例して少しずつ変化させる
   float sAmount = shiftAmount * (float)(sigma / blurSigmas[0]);

   cv::Mat r_shifted = shiftFloat(red, std::cos(glow_dir_rad) * sAmount, std::sin(glow_dir_rad) * sAmount);
   cv::Mat g_shifted = shiftFloat(green, std::cos(glow_dir_rad + 2.0f) * sAmount, std::sin(glow_dir_rad + 2.0f) * sAmount);
   cv::Mat b_shifted = shiftFloat(blue, std::cos(glow_dir_rad - 2.0f) * sAmount, std::sin(glow_dir_rad - 2.0f) * sAmount);

   cv::GaussianBlur(r_shifted, r_shifted, cv::Size(0, 0), sigma);
   cv::GaussianBlur(g_shifted, g_shifted, cv::Size(0, 0), sigma);
   cv::GaussianBlur(b_shifted, b_shifted, cv::Size(0, 0), sigma);

   cv::Mat r_mix = 0.7f * r_shifted + 0.15f * g_shifted + 0.15f * b_shifted;
   cv::Mat g_mix = 0.7f * g_shifted + 0.15f * r_shifted + 0.15f * b_shifted;
   cv::Mat b_mix = 0.7f * b_shifted + 0.15f * g_shifted + 0.15f * r_shifted;

   cv::Mat glowPart;
   cv::merge(std::vector<cv::Mat>{b_mix, g_mix, r_mix}, glowPart);

   // σが大きいほど弱くする（重ねる光の層）
   glowPart *= (float)(1.0 / sigma);

   glowTotal += glowPart;
  }

  // 6. 強度調整（お好みで）
  glowTotal *= 1.2f;

  // 7. 元画像に加算（clamp）
  for (int y = 0; y < mat.rows; ++y)
  {
   cv::Vec4f* row = mat.ptr<cv::Vec4f>(y);
   const cv::Vec3f* glow_row = glowTotal.ptr<cv::Vec3f>(y);
   for (int x = 0; x < mat.cols; ++x)
   {
	row[x][0] = std::min(row[x][0] + glow_row[x][0], 1.0f);
	row[x][1] = std::min(row[x][1] + glow_row[x][1], 1.0f);
	row[x][2] = std::min(row[x][2] + glow_row[x][2], 1.0f);
	// alphaはそのまま
   }
  }
  qint64 elapsed = timer.elapsed();  // 単位: ミリ秒
  qDebug() << "処理時間:" << elapsed << "ms";

 }

 void SpectralGlow::Process6(cv::Mat& mat)
 {
  CV_Assert(mat.type() == CV_32FC4); // BGRA float

  // 1. HSVに変換して輝度と彩度を取得
  cv::Mat bgrMat;
  cv::cvtColor(mat, bgrMat, cv::COLOR_BGRA2BGR);

  cv::Mat hsvMat;
  cv::cvtColor(bgrMat, hsvMat, cv::COLOR_BGR2HSV);

  std::vector<cv::Mat> hsvChannels;
  cv::split(hsvMat, hsvChannels);
  cv::Mat& hue = hsvChannels[0];         // 0-179
  cv::Mat& saturation = hsvChannels[1];  // 0-255
  cv::Mat& value = hsvChannels[2];       // 0-255

  // 2. 輝度マスクと彩度マスクを作る（正規化してfloatに変換）
  cv::Mat valueF, saturationF;
  value.convertTo(valueF, CV_32F, 1.0 / 255.0);
  saturation.convertTo(saturationF, CV_32F, 1.0 / 255.0);

  cv::Mat brightMask, satMask;
  cv::threshold(valueF, brightMask, 0.15f, 1.0f, cv::THRESH_BINARY);
  cv::threshold(saturationF, satMask, 0.2f, 1.0f, cv::THRESH_BINARY);

  cv::Mat mask = brightMask.mul(satMask); // 対象範囲マスク

  // 3. グレースケール化して勾配方向を計算
  cv::Mat gray;
  cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);

  cv::Mat grad_x, grad_y;
  cv::Sobel(gray, grad_x, CV_32F, 1, 0, 3);
  cv::Sobel(gray, grad_y, CV_32F, 0, 1, 3);

  cv::Mat angle;
  cv::phase(grad_x, grad_y, angle, true);

  // 4. 全体の平均勾配方向を計算し、色ずらし基本方向を決める
  double sum_sin = 0, sum_cos = 0;
  int count = angle.rows * angle.cols;
  for (int y = 0; y < angle.rows; ++y)
  {
   const float* a = angle.ptr<float>(y);
   for (int x = 0; x < angle.cols; ++x)
   {
	float rad = a[x] * static_cast<float>(CV_PI / 180.0);
	sum_cos += std::cos(rad);
	sum_sin += std::sin(rad);
   }
  }
  float avg_rad = std::atan2(sum_sin / count, sum_cos / count);
  float glow_dir_rad = avg_rad + static_cast<float>(CV_PI / 2.0);

  int baseShift = 5;

  // 5. 色ずらし方向群（ラジアン）
  std::vector<float> directionOffsets = { 0.0f, 0.52f, -0.52f, 1.05f, -1.05f }; // 0°, ±30°, ±60°

  // 6. 色チャネル用マスク（マスク適用のため同じサイズのfloatマット作成）
  cv::Mat redMask(mask.size(), CV_32F, cv::Scalar(0));
  cv::Mat greenMask = redMask.clone();
  cv::Mat blueMask = redMask.clone();

  // 7. maskを元に各チャネルのマスク作成（全体は同じだが後で変えてもOK）
  for (int y = 0; y < mask.rows; ++y)
  {
   const float* m = mask.ptr<float>(y);
   float* r = redMask.ptr<float>(y);
   float* g = greenMask.ptr<float>(y);
   float* b = blueMask.ptr<float>(y);
   for (int x = 0; x < mask.cols; ++x)
   {
	float v = m[x];
	r[x] = v;
	g[x] = v;
	b[x] = v;
   }
  }

  // 8. シフト関数
  auto shiftFloat = [&](const cv::Mat& src, float dx, float dy) -> cv::Mat {
   cv::Mat dst;
   cv::Mat M = (cv::Mat_<float>(2, 3) << 1, 0, dx, 0, 1, dy);
   cv::warpAffine(src, dst, M, src.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, 0);
   return dst;
   };

  // 9. RNG初期化（ノイズ用）
  cv::RNG rng(cv::getTickCount());

  // 10. マルチ方向・マルチスケールブラー用σリスト
  std::vector<double> blurSigmas = { 6.0, 12.0, 24.0 };

  // 11. グロー合成用3チャネル浮動小数点Mat
  cv::Mat glowTotal = cv::Mat::zeros(mat.size(), CV_32FC3);

  // 12. マルチスケール＋マルチ方向ループ
  for (double sigma : blurSigmas)
  {
   float scale = static_cast<float>(sigma / blurSigmas[0]);
   for (float angleOffset : directionOffsets)
   {
	float angle = glow_dir_rad + angleOffset;

	// ノイズ成分(-0.3〜+0.3)
	float noiseX = rng.uniform(-0.3f, 0.3f);
	float noiseY = rng.uniform(-0.3f, 0.3f);

	float shiftX = (std::cos(angle) + noiseX) * baseShift * scale;
	float shiftY = (std::sin(angle) + noiseY) * baseShift * scale;

	// チャネルごとにシフト＋ブラー
	cv::Mat r_shifted = shiftFloat(redMask, shiftX, shiftY);
	cv::Mat g_shifted = shiftFloat(greenMask, shiftX, shiftY);
	cv::Mat b_shifted = shiftFloat(blueMask, shiftX, shiftY);

	cv::GaussianBlur(r_shifted, r_shifted, cv::Size(0, 0), sigma);
	cv::GaussianBlur(g_shifted, g_shifted, cv::Size(0, 0), sigma);
	cv::GaussianBlur(b_shifted, b_shifted, cv::Size(0, 0), sigma);

	// 色混合（やや赤強め）
	cv::Mat r_mix = 0.7f * r_shifted + 0.15f * g_shifted + 0.15f * b_shifted;
	cv::Mat g_mix = 0.7f * g_shifted + 0.15f * r_shifted + 0.15f * b_shifted;
	cv::Mat b_mix = 0.7f * b_shifted + 0.15f * r_shifted + 0.15f * g_shifted;

	cv::Mat glowPart;
	cv::merge(std::vector<cv::Mat>{b_mix, g_mix, r_mix}, glowPart);

	glowPart *= (float)(1.0 / sigma); // 大きいσは薄め

	glowTotal += glowPart;
   }
  }

  // 13. 強度調整
  glowTotal *= 1.7f;

  // 14. 元画像に加算（clamp）
  for (int y = 0; y < mat.rows; ++y)
  {
   cv::Vec4f* row = mat.ptr<cv::Vec4f>(y);
   const cv::Vec3f* glowRow = glowTotal.ptr<cv::Vec3f>(y);
   for (int x = 0; x < mat.cols; ++x)
   {
	row[x][0] = std::min(row[x][0] + glowRow[x][0], 1.0f);
	row[x][1] = std::min(row[x][1] + glowRow[x][1], 1.0f);
	row[x][2] = std::min(row[x][2] + glowRow[x][2], 1.0f);
	// アルファは変えず
   }
  }
 }

};