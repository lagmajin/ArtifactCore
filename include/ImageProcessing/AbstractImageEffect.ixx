module;
#include <utility>

export module ImageProcessing;

export import :Monochrome;
export import :NegativeCV;
export import :AffineTransform;
export import :AntiAliasing;
export import :Posterize;
// export import :Vignette; // Vignette.ixx missing

// Sub-modules (imported as full module names)
// export import ImageProcessing.ColorTransform.LevelsCurves;
// export import ImageProcessing.ColorTransform.HueSaturation;

//export import :Halide;
export import ImageProcessing.NegateCS;
//export import :ImageTransform;
import ImageF32x4;



export namespace ArtifactCore {



class AbstractImageEffect {
private:

public:
 AbstractImageEffect();
 virtual ~AbstractImageEffect() = default;
 //virtual void process(ImageF32x4_RGBA& image) = 0;

};






};
