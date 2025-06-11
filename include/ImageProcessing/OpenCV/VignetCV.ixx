module;
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
export module ImageProcessing:Vignette;


export namespace ArtifactCore {

 LIBRARY_DLL_API cv::Mat vignetteEffect(const cv::Mat& input);

}