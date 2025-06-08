module;

#include "../../Define/DllExportMacro.hpp"

#include <opencv2/opencv.hpp>


export module Analyze:OpticalFlow;





export namespace ArtifactCore {

 LIBRARY_DLL_API class OpticalFlowResult {
 private:
  cv::Mat flow; // CV_32FC2 �̓����x�N�g���}�b�v

 public:
  OpticalFlowResult(const cv::Mat& flow_) : flow(flow_) {}

  // ���ϓ����x�N�g�����v�Z
  cv::Point2f getAverageFlow() const;

  // �����̑傫����臒l�ȏ�̉�f���}�X�N�ŕԂ�
  cv::Mat getMagnitudeMask(float threshold) const;

  // �����q�X�g�O�����i8�����j
  std::vector<int> getDirectionHistogram(int bins = 8) const;

  // �����x�N�g����F�ŉ��������摜���쐬
  cv::Mat visualizeFlow() const;
 };

}