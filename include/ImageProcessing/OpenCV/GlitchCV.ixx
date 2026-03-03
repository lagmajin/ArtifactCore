module;
#include <opencv2/opencv.hpp>
#include "../../Define/DllExportMacro.hpp"
export module ImageProcessing.GlitchCV;

export namespace ArtifactCore {

    /**
     * @brief Digital glitch effect parameters.
     */
    struct GlitchParams {
        float intensity   = 0.5f;   // 0..1 Overall glitch strength
        float blockSize   = 16.0f;  // Pixel size of glitch blocks
        float rgbShift    = 8.0f;   // Pixels of RGB channel shift
        float scanlines   = 0.3f;   // Scanline overlay intensity
        float noise       = 0.1f;   // Random noise amount
        int   seed        = 42;     // Random seed for reproducibility
    };

    /**
     * @brief Apply digital glitch effect (block displacement + RGB shift + scanlines).
     */
    LIBRARY_DLL_API cv::Mat glitchEffect(const cv::Mat& input, const GlitchParams& params = {});

    /**
     * @brief Apply RGB channel separation/shift only.
     */
    LIBRARY_DLL_API cv::Mat rgbShiftEffect(const cv::Mat& input, float shiftX = 5.0f, float shiftY = 0.0f);

}