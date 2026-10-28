module;
#include <utility>
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
export module VHS_CV;

export namespace ArtifactCore {

    /**
     * @brief VHS / analog video tape degradation effect parameters.
     */
    struct VHSParams {
        float trackingError = 0.3f;  // Horizontal tracking offset intensity
        float noiseAmount   = 0.15f; // Amount of tape noise
        float colorBleed    = 0.5f;  // Chroma blur / bleed amount
        float scanlineGap   = 2.0f;  // Scanline gap in pixels
        float scanlineIntensity = 0.2f; // Scanline dark intensity (0-1)
        float wobble        = 0.2f;  // Horizontal line wobble
        float saturation    = 0.8f;  // Color desaturation (< 1.0 = washed out)
        float sharpness     = 0.6f;  // Overall sharpness reduction
        float bottomJitter  = 0.5f;  // VCR head-switching noise at bottom
        float evolution     = 0.0f;  // Time / frame progression
        int   seed          = 0;     // Random seed
        bool  showOsd       = false; // Overlay retro VHS OSD (PLAY / timestamp)
        int   osdMode       = 0;     // 0: PLAY, 1: REC, 2: SP, 3: AUTO
    };

    /**
     * @brief Apply full VHS tape degradation effect.
     */
    LIBRARY_DLL_API cv::Mat vhsEffect(const cv::Mat& input, const VHSParams& params = {});

    /**
     * @brief Apply scanline overlay only.
     */
    LIBRARY_DLL_API cv::Mat scanlineOverlay(const cv::Mat& input, float gap = 2.0f, float intensity = 0.3f);

}
