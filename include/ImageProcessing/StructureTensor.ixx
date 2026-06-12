module;
#include "../Define/DllExportMacro.hpp"
#include <vector>

export module ImageProcessing:StructureTensor;

import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct TensorField {
    int width = 0;
    int height = 0;

    // Buffer containing orientation angles in radians (-PI/2 to PI/2)
    std::vector<float> angles;

    // Buffer containing local coherence/anisotropy values (0.0 to 1.0)
    // 1.0 means highly directional edge, 0.0 means isotropic flat or noise area.
    std::vector<float> coherence;

    // Energy / Strength of the gradient
    std::vector<float> magnitude;
};

class LIBRARY_DLL_API StructureTensor {
public:
    StructureTensor() = default;
    ~StructureTensor() = default;

    // Compute the structure tensor fields for the image
    // rGaussianWidth: Sobel derivative smoothing (noise suppression)
    // tGaussianWidth: Tensor integration scale (averaging window for neighborhood flow)
    TensorField analyze(const ImageF32x4_RGBA& image, float rGaussianWidth = 1.0f, float tGaussianWidth = 3.0f);

    // Analyze raw cv::Mat (CV_32FC4 in BGRA format or CV_32FC1 grayscale)
    TensorField analyzeMat(const void* cvMatPtr, float rGaussianWidth = 1.0f, float tGaussianWidth = 3.0f);
};

} // namespace ArtifactCore
