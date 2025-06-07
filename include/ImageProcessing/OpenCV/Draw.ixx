module;
#include <opencv2/opencv.hpp>
#include "../../Define/DllExportMacro.hpp"
export module Draw;



export namespace ArtifactCore {

  export LIBRARY_DLL_API void drawStar5(cv::Mat& img,
  cv::Scalar edgeColor,
  int edgeThickness,
  cv::Scalar fillColor,
  float scale);




 void drawStar6(cv::Mat& img,
  cv::Scalar edgeColor,
  int edgeThickness = 1,
  cv::Scalar fillColor = cv::Scalar(-1, -1, -1, -1),
  float scale = 1.0f);





}