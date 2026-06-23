module;

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "../Define/DllExportMacro.hpp"

export module ImageProcessing;

export import :Monochrome;
export import :NegativeCV;
export import :AffineTransform;
export import :AntiAliasing;
export import :Posterize;
export import ImageProcessing.ProceduralTexture;
export import ImageProcessing.ColorTransform.Tritone;
export import ImageProcessing.ColorTransform.Colorama;
export import ImageProcessing.ColorTransform.PhotoFilter;
export import ImageProcessing.ColorTransform.ColorBalance;
export import ImageProcessing.ColorTransform.LevelsCurves;
export import ImageProcessing.ColorTransform.ChannelMixer;
export import ImageProcessing.ColorTransform.SelectiveColor;
export import ImageProcessing.ColorTransform.GradientRamp;
export import ImageProcessing.ColorTransform.Fill;
export import :MotionTrail;
export import :Halftone;
export import :TiltShift;
export import :AnamorphicFlare;
export import :Duotone;
export import :EdgeEcho;
export import :ChromaSpread;
export import :StructureTensor;
export import :ChromaSpreadGlow;
export import :AnisotropicFlowBlur;
export import :BroadcastColors;
export import :ChromaKey;
export import :Echo;
export import :Emboss;
export import :LeaveColor;
export import :LumaKey;
export import :Median;
export import :PosterizeTime;
export import :Scatter;
export import :SimpleChoker;
export import :StrobeLight;
export import :Threshold;
export import :VectorFlowGlitch;

export import ImageProcessing.BroadcastColorsCS;
export import ImageProcessing.ChromaKeyCS;
export import ImageProcessing.EmbossCS;
export import ImageProcessing.LeaveColorCS;
export import ImageProcessing.NegateCS;
export import ImageProcessing.ScatterCS;
export import ImageProcessing.SimpleChokerCS;
import ImageF32x4;

export namespace ArtifactCore {

struct EffectParamDef {
    std::string name;
    std::string label;
    double defaultValue = 0.0;
    double minValue = 0.0;
    double maxValue = 1.0;
    double step = 0.01;
};

struct EffectROI {
    int expansionPixels = 0;
    bool requiresFullFrame = false;
};

class LIBRARY_DLL_API AbstractImageEffect {
public:
    AbstractImageEffect() = default;
    AbstractImageEffect(const AbstractImageEffect&) = delete;
    AbstractImageEffect& operator=(const AbstractImageEffect&) = delete;
    AbstractImageEffect(AbstractImageEffect&&) noexcept = default;
    AbstractImageEffect& operator=(AbstractImageEffect&&) noexcept = default;
    virtual ~AbstractImageEffect() = default;

    virtual void process(ImageF32x4_RGBA& image) = 0;
    virtual std::string name() const = 0;

    virtual std::vector<EffectParamDef> parameters() const;
    virtual void setParam(const std::string& name, double value);
    virtual double getParam(const std::string& name) const;

    virtual EffectROI roiHint() const;

    void setNext(std::shared_ptr<AbstractImageEffect> next) { next_ = std::move(next); }
    std::shared_ptr<AbstractImageEffect> next() const { return next_; }

    void chainProcess(ImageF32x4_RGBA& image);

protected:
    std::shared_ptr<AbstractImageEffect> next_;
    std::vector<std::pair<std::string, double>> paramValues_;

    bool findParamIndex(const std::string& name, size_t& idx) const;
};

} // namespace ArtifactCore
