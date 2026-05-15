module;
#include <utility>
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
export module LiftCV;

export namespace ArtifactCore {

    /**
     * @brief Lift / Gamma / Gain color grading (三点カラーコレクション).
     * Operates on float RGBA images (CV_32FC4).
     */
    LIBRARY_DLL_API cv::Mat liftGammaGain(const cv::Mat& input,
                                           cv::Vec3f lift  = {0.0f, 0.0f, 0.0f},   // Shadow offset R,G,B
                                           cv::Vec3f gamma = {1.0f, 1.0f, 1.0f},   // Midtone power
                                           cv::Vec3f gain  = {1.0f, 1.0f, 1.0f});  // Highlight multiplier

    /**
     * @brief Simple shadow lift: raises the black level.
     */
    LIBRARY_DLL_API cv::Mat shadowLift(const cv::Mat& input, float amount = 0.05f);

}
