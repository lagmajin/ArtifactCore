module;
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
export module ImageProcessing:Monochrome;



export namespace ArtifactCore {

 LIBRARY_DLL_API cv::Mat convertToPhysicallyCorrectGrayscale(const cv::Mat& src_bgr);
 void toLuminanceGrayRGBA(const cv::Mat& src, cv::Mat& dst);
}