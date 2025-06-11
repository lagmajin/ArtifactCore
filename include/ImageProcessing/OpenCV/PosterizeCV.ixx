module;
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>


export module ImageProcessing:Posterize;



export namespace  ArtifactCore{

 LIBRARY_DLL_API cv::Mat posterizeEffect(const cv::Mat& input, int levels = 4);



}