module ;

#include <opencv2/opencv.hpp>
module Analyze.OpticalFlow;

import std;

namespace ArtifactCore {

class OpticalFlowResult::Impl
{
private:

public:
 Impl();
 ~Impl();
 cv::Mat flow;
};

OpticalFlowResult::Impl::Impl()
{

}

OpticalFlowResult::Impl::~Impl()
{

}

 cv::Mat OpticalFlowResult::getMagnitudeMask(float threshold) const
 {
  cv::Mat mag(flow.size(), CV_32F);
  for (int y = 0; y < flow.rows; y++) {
   for (int x = 0; x < flow.cols; x++) {
	cv::Point2f f = flow.at<cv::Point2f>(y, x);
	mag.at<float>(y, x) =(float) cv::norm(f);
   }
  }
  cv::Mat mask;
  cv::threshold(mag, mask, threshold, 255, cv::THRESH_BINARY);
  mask.convertTo(mask, CV_8U);
  return mask;
 }

 cv::Point2f OpticalFlowResult::getAverageFlow() const
 {
  cv::Scalar avg = cv::mean(flow);
  return cv::Point2f(static_cast<float>(avg[0]), static_cast<float>(avg[1]));
 }

 std::vector<int> OpticalFlowResult::getDirectionHistogram(int bins /*= 8*/) const
 {
  std::vector<int> hist(bins, 0);
  for (int y = 0; y < flow.rows; y++) {
   for (int x = 0; x < flow.cols; x++) {
	cv::Point2f f = flow.at<cv::Point2f>(y, x);
	float angle = std::atan2(f.y, f.x); // -pi..pi
	if (cv::norm(f) < 1e-3) continue;   // 動きなしは除外
	int bin = static_cast<int>((angle + CV_PI) / (2 * CV_PI) * bins) % bins;
	hist[bin]++;
   }
  }
  return hist;
 }

 cv::Mat OpticalFlowResult::visualizeFlow() const
 {
  cv::Mat hsv(flow.size(), CV_8UC3);
  for (int y = 0; y < flow.rows; y++) {
   for (int x = 0; x < flow.cols; x++) {
	cv::Point2f f = flow.at<cv::Point2f>(y, x);
	float mag = (float)cv::norm(f);
	float angle = std::atan2(f.y, f.x);

	// 色相(H)は方向、彩度(S)は最大値、明度(V)は大きさ
	uchar H = static_cast<uchar>((angle + CV_PI) * 180 / CV_PI); // 0..360度を0..180にスケール
	uchar S = 255;
	uchar V =(uchar) std::min(255.0f, mag * 10);

	hsv.at<cv::Vec3b>(y, x) = cv::Vec3b(H, S, V);
   }
  }
  cv::Mat bgr;
  cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
  return bgr;
 }

 OpticalFlowResult::OpticalFlowResult(const cv::Mat& flow_) :impl_(new Impl()),flow(flow_)
 {

 }

 OpticalFlowResult::~OpticalFlowResult()
 {
  delete impl_;
 }


}