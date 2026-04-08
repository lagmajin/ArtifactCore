module;
#include <utility>
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
export module Image:ImageProcessing;

export namespace ArtifactCore {

    /**
     * @brief Halftone dot pattern effect.
     * Simulates print-style halftone rendering.
     */
    struct HalftoneParams {
        float dotSize       = 6.0f;    // Size of halftone dots
        float angle         = 45.0f;   // Screen angle in degrees
        float contrast      = 1.0f;    // Dot contrast (0..2)
        bool  colorMode     = true;    // true = CMYK-style color halftone, false = mono
        float cmykAngles[4] = {15.0f, 75.0f, 0.0f, 45.0f}; // C, M, Y, K screen angles
    };

    /**
     * @brief Apply halftone dot pattern effect.
     */
    LIBRARY_DLL_API cv::Mat halftoneEffect(const cv::Mat& input, const HalftoneParams& params = {});

    /**
     * @brief Simple monochrome halftone using ordered dithering.
     */
    LIBRARY_DLL_API cv::Mat orderedDither(const cv::Mat& input, int matrixSize = 4);

}
