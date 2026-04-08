module;
#include <utility>
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
export module ImageProcessing:AntiAliasing;

export namespace ArtifactCore {

    /**
     * @brief FXAA-style anti-aliasing for rendered images.
     * Detects edges based on luminance contrast and smooths them.
     */
    LIBRARY_DLL_API cv::Mat antiAlias(const cv::Mat& input, float strength = 1.0f);

    /**
     * @brief Supersampling anti-aliasing: render at higher res, then downsample.
     * factor: 2 = 2x2 SSAA, 4 = 4x4 SSAA
     */
    LIBRARY_DLL_API cv::Mat antiAliasSuperSample(const cv::Mat& input, int factor = 2);

}
