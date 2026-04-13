module;

#include <opencv2/opencv.hpp>
#include "../../../include/Define/DllExportMacro.hpp"
#include <QtConcurrent>
#include <numeric>

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
module ImageProcessing;


import :Monochrome;


namespace ArtifactCore {

 float srgbToLinear(float c) {
  if (c <= 0.04045f) return c / 12.92f;
  return std::pow((c + 0.055f) / 1.055f, 2.4f);
 }

 // Linear RGB → sRGB
 float linearToSrgb(float c) {
  if (c <= 0.0031308f) return 12.92f * c;
  return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
 }

 // 物理的に正しいグレースケール変換 (BT.709)
 LIBRARY_DLL_API cv::Mat convertToPhysicallyCorrectGrayscale(const cv::Mat& src_bgr) {
  CV_Assert(src_bgr.type() == CV_8UC3);

  const int rows = src_bgr.rows;
  const int cols = src_bgr.cols;
  cv::Mat gray(rows, cols, CV_8UC1);

  // sRGB → Linear → Grayscale → sRGB を行単位で並列化
  std::vector<uchar> rowResults(rows * cols);

  QVector<int> rowsToProcess(rows);
  for (int i = 0; i < rows; ++i) {
   rowsToProcess[i] = i;
  }

  QtConcurrent::blockingMap(rowsToProcess, [&](int y) {
   for (int x = 0; x < cols; ++x) {
    cv::Vec3b bgr = src_bgr.at<cv::Vec3b>(y, x);

    // BGR → RGB変換 (0-1)
    float r_srgb = bgr[2] / 255.0f;
    float g_srgb = bgr[1] / 255.0f;
    float b_srgb = bgr[0] / 255.0f;

    // sRGB → 線形
    float r_lin = srgbToLinear(r_srgb);
    float g_lin = srgbToLinear(g_srgb);
    float b_lin = srgbToLinear(b_srgb);

    // BT.709重み付け
    float y_lin = 0.2126f * r_lin + 0.7152f * g_lin + 0.0722f * b_lin;

    // 線形 → sRGB
    float y_srgb = linearToSrgb(y_lin);

    // 0-255に変換
    rowResults[y * cols + x] = static_cast<uchar>(std::round(y_srgb * 255.0f));
   }
  });

  // 結果をgrayにコピー
  for (int y = 0; y < rows; ++y) {
   for (int x = 0; x < cols; ++x) {
    gray.at<uchar>(y, x) = rowResults[y * cols + x];
   }
  }

  return gray;
 }

 void toLuminanceGrayRGBA(const cv::Mat& src, cv::Mat& dst)
 {
  CV_Assert(src.type() == CV_8UC4); // BGRA

  cv::Mat floatSrc;
  src.convertTo(floatSrc, CV_32FC4);

  // 物理的に正しいグレースケール変換 (BT.709)
  const cv::Matx<float, 4, 4> transformMatrix = {
    0.0722f, 0.7152f, 0.2126f, 0.0f,
    0.0722f, 0.7152f, 0.2126f, 0.0f,
    0.0722f, 0.7152f, 0.2126f, 0.0f,
    0.0f,    0.0f,    0.0f,    1.0f
  };

  cv::Mat floatDst;
  cv::transform(floatSrc, floatDst, transformMatrix);

  floatDst.convertTo(dst, CV_8UC4);
 }


}
