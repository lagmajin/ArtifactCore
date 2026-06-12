module;
#include "../Define/DllExportMacro.hpp"
#include <vector>

export module ImageProcessing:NeuromorphicDisplace;

import Image.ImageF32x4_RGBA;
import FloatRGBA;

export namespace ArtifactCore {

struct NeuromorphicDisplaceSettings {
    // 3D emboss height/relief scaling factor
    float depth = 5.0f;

    // Softness of the 3D rounded edges
    float softness = 4.0f;

    // Polar coordinates for the directional light source
    float lightAngle = 135.0f;     // in degrees (0-360)
    float lightElevation = 45.0f; // elevation in degrees (0-90)

    // Phong Shading properties
    float ambient = 0.3f;
    float diffuse = 0.7f;
    float specular = 0.5f;
    float shininess = 16.0f;      // highlight focus/sharpness

    // Refraction intensity (how much the background deforms beneath the shape)
    float refraction = 10.0f;

    // Overall blend factor (0.0 = original image, 1.0 = fully shaded 3D relief)
    float blend = 1.0f;
};

class LIBRARY_DLL_API NeuromorphicDisplace {
public:
    NeuromorphicDisplace() = default;
    ~NeuromorphicDisplace() = default;

    // Apply NeuromorphicDisplace directly to the image
    void process(ImageF32x4_RGBA& image, const NeuromorphicDisplaceSettings& settings);
};

} // namespace ArtifactCore
