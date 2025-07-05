module;
#include <opencv2/opencv.hpp>
#include "../../Define/DllExportMacro.hpp"

export module ImageProcessing.SpectralGlow;

import std;

export namespace ArtifactCore {


 class LIBRARY_DLL_API  SpectralGlow {
 private:

 public:
  SpectralGlow();
  ~SpectralGlow();
  void Process(cv::Mat& mat);
  void Process2(cv::Mat& mat);
  void Process3(cv::Mat& mat);
  void Process4(cv::Mat& mat);
  void ElegantGlow(cv::Mat& mat);
  void Process6(cv::Mat& mat);
 };










};
