module;
#include "../Define/DllExportMacro.hpp"
#include <vector>

export module ImageProcessing:ChromaSpread;

import Image.ImageF32x4_RGBA;
import FloatRGBA;

export namespace ArtifactCore {

struct ChromaSpreadSettings {
    // Radial Chromatic Aberration (aberration from the center)
    // Scale factor multiplier for R, G, B channels respectively.
    // e.g. R = 1.01f, G = 1.00f, B = 0.99f (1.0f means no scaling change)
    float redScale = 1.0f;
    float greenScale = 1.0f;
    float blueScale = 1.0f;

    // Linear Shift (Directional offset per channel)
    // Shift distance in pixels and angle in degrees
    float shiftAmount = 0.0f;
    float shiftAngle = 0.0f; 

    // Spectral dispersion steps (for extra smooth rainbow glow/trails)
    // If steps > 1, intermediate wavelengths will be interpolated.
    int dispersionSteps = 1;
};

class LIBRARY_DLL_API ChromaSpread {
public:
    ChromaSpread() = default;
    ~ChromaSpread() = default;

    // Apply ChromaSpread transformation to the image in-place
    void process(ImageF32x4_RGBA& image, const ChromaSpreadSettings& settings);

    // Process a raw cv::Mat in BGRA layout (CV_32FC4)
    // This is useful for internal steps in larger pipelines (like Glow)
    void processMat(void* cvMatPtr, const ChromaSpreadSettings& settings);
};

} // namespace ArtifactCore
