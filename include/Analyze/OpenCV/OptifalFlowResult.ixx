module;

#include "../../Define/DllExportMacro.hpp"

#include <opencv2/opencv.hpp>


export module Analyze:OpticalFlow;





export namespace ArtifactCore {

 LIBRARY_DLL_API class OpticalFlowResult {
 private:
  cv::Mat flow; // CV_32FC2 の動きベクトルマップ

 public:
  OpticalFlowResult(const cv::Mat& flow_) : flow(flow_) {}

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