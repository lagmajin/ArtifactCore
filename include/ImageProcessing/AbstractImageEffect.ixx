module;
#include <utility>

export module ImageProcessing;

export import :Monochrome;
export import :NegativeCV;
export import :AffineTransform;
export import :AntiAliasing;
export import :Posterize;
export import ImageProcessing.ProceduralTexture;
// export import :Vignette; // Vignette.ixx missing

// Sub-modules (imported as full module names)
// export import ImageProcessing.ColorTransform.LevelsCurves;
// export import ImageProcessing.ColorTransform.HueSaturation;
export import ImageProcessing.ColorTransform.Tritone;
export import ImageProcessing.ColorTransform.Colorama;
export import ImageProcessing.ColorTransform.PhotoFilter;
export import ImageProcessing.ColorTransform.ColorBalance;
export import ImageProcessing.ColorTransform.LevelsCurves;
export import ImageProcessing.ColorTransform.ChannelMixer;
export import ImageProcessing.ColorTransform.SelectiveColor;
export import ImageProcessing.ColorTransform.GradientRamp;
export import ImageProcessing.ColorTransform.Fill;

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
