module;
#include <utility>
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
export module Noise;

export namespace ArtifactCore {

    enum class NoiseType {
        Gaussian,       // Normal distribution noise
        Uniform,        // Uniform random noise
        SaltAndPepper,  // Black & white specks
        Perlin,         // Smooth Perlin noise texture
        FilmGrain       // Film grain simulation
    };

    /**
     * @brief Add noise to an image.
     * @param input Source image (float or uint8)
     * @param type Noise type
     * @param amount Noise intensity (0..1)
     * @param monochrome If true, same noise for all channels (luminance noise)
     * @param seed Random seed
     */
    LIBRARY_DLL_API cv::Mat addNoise(const cv::Mat& input, NoiseType type = NoiseType::Gaussian,
                                      float amount = 0.1f, bool monochrome = false, int seed = -1);

    /**
     * @brief Generate a pure noise texture (no input required).
     */
    LIBRARY_DLL_API cv::Mat generateNoiseTexture(int width, int height, NoiseType type = NoiseType::Gaussian,
                                                  float amount = 1.0f, int seed = -1);

}
