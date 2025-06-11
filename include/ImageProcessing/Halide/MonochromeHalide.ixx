module;
//#include <DllExportMacro.hpp>
#include <Halide.h>
#include "../../Define/DllExportMacro.hpp"

#include <opencv2/opencv.hpp>
export module ImageProcessing:Halide;



export namespace ArtifactCore {
 using namespace Halide;

 Func makeLuminanceFunc(const Func& inputRGBA);

 LIBRARY_DLL_API cv::Mat halideTest2(cv::Mat&src);
 LIBRARY_DLL_API  cv::Mat halideTestMinimal(const cv::Mat& inputRGBA32F);
 LIBRARY_DLL_API  cv::Mat halideTestMinimal2(const cv::Mat& inputRGBA32F);

}