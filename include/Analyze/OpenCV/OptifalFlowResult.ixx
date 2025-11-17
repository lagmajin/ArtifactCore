module;

#include "../../Define/DllExportMacro.hpp"

#include <opencv2/opencv.hpp>


export module Analyze.OpticalFlow;





export namespace ArtifactCore {

  class LIBRARY_DLL_API OpticalFlowResult {
 private:
  cv::Mat flow; // CV_32FC2 の動きベクトルマップ

  class Impl;
  Impl* impl_;
 public:
  OpticalFlowResult(const cv::Mat& flow_);
  ~OpticalFlowResult();
  // 平均動きベクトルを計算
  cv::Point2f getAverageFlow() const;

  // 動きの大きさの閾値以上の画素をマスクで返す
  cv::Mat getMagnitudeMask(float threshold) const;

  // 方向ヒストグラム（8方向）
  std::vector<int> getDirectionHistogram(int bins = 8) const;

  // 動きベクトルを色で可視化した画像を作成
  cv::Mat visualizeFlow() const;
 };

}