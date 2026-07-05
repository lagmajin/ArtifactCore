module;
#include <utility>
#include <QString>

export module Layer.Blend;

export namespace ArtifactCore {

/**
 * @brief Blending modes for compositing layers.
 */
enum class BlendMode {
    Normal = 0,
    Add,
    Subtract,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity,
    LinearBurn,
    Divide,
    PinLight,
    VividLight,
    LinearLight,
    HardMix,
    Dissolve,
    DancingDissolve,
    ClassicColorBurn,
    LinearDodge,
    ClassicColorDodge,
    ClassicDifference,
    StencilAlpha,
    StencilLuma,
    SilhouetteAlpha,
    SilhouetteLuma
};

// Legacy compatibility for existing Artifact layer code paths.
// Keep this until all call sites migrate to BlendMode.
enum class LAYER_BLEND_TYPE {
    BLEND_NORMAL = static_cast<int>(BlendMode::Normal),
    BLEND_ADD = static_cast<int>(BlendMode::Add),
    BLEND_SUBTRACT = static_cast<int>(BlendMode::Subtract),
    BLEND_MULTIPLY = static_cast<int>(BlendMode::Multiply),
    BLEND_SCREEN = static_cast<int>(BlendMode::Screen),
    BLEND_OVERLAY = static_cast<int>(BlendMode::Overlay),
    BLEND_DARKEN = static_cast<int>(BlendMode::Darken),
    BLEND_LIGHTEN = static_cast<int>(BlendMode::Lighten),
    BLEND_COLOR_DODGE = static_cast<int>(BlendMode::ColorDodge),
    BLEND_COLOR_BURN = static_cast<int>(BlendMode::ColorBurn),
    BLEND_HARD_LIGHT = static_cast<int>(BlendMode::HardLight),
    BLEND_SOFT_LIGHT = static_cast<int>(BlendMode::SoftLight),
    BLEND_DIFFERENCE = static_cast<int>(BlendMode::Difference),
    BLEND_EXCLUSION = static_cast<int>(BlendMode::Exclusion),
    BLEND_HUE = static_cast<int>(BlendMode::Hue),
    BLEND_SATURATION = static_cast<int>(BlendMode::Saturation),
    BLEND_COLOR = static_cast<int>(BlendMode::Color),
    BLEND_LUMINOSITY = static_cast<int>(BlendMode::Luminosity),
    BLEND_LINEAR_BURN = static_cast<int>(BlendMode::LinearBurn),
    BLEND_DIVIDE = static_cast<int>(BlendMode::Divide),
    BLEND_PIN_LIGHT = static_cast<int>(BlendMode::PinLight),
    BLEND_VIVID_LIGHT = static_cast<int>(BlendMode::VividLight),
    BLEND_LINEAR_LIGHT = static_cast<int>(BlendMode::LinearLight),
    BLEND_HARD_MIX = static_cast<int>(BlendMode::HardMix),
    BLEND_DISSOLVE = static_cast<int>(BlendMode::Dissolve),
    BLEND_DANCING_DISSOLVE = static_cast<int>(BlendMode::DancingDissolve),
    BLEND_CLASSIC_COLOR_BURN = static_cast<int>(BlendMode::ClassicColorBurn),
    BLEND_LINEAR_DODGE = static_cast<int>(BlendMode::LinearDodge),
    BLEND_CLASSIC_COLOR_DODGE = static_cast<int>(BlendMode::ClassicColorDodge),
    BLEND_CLASSIC_DIFFERENCE = static_cast<int>(BlendMode::ClassicDifference),
    BLEND_STENCIL_ALPHA = static_cast<int>(BlendMode::StencilAlpha),
    BLEND_STENCIL_LUMA = static_cast<int>(BlendMode::StencilLuma),
    BLEND_SILHOUETTE_ALPHA = static_cast<int>(BlendMode::SilhouetteAlpha),
    BLEND_SILHOUETTE_LUMA = static_cast<int>(BlendMode::SilhouetteLuma)
};

inline BlendMode toBlendMode(const LAYER_BLEND_TYPE legacy) {
    return static_cast<BlendMode>(static_cast<int>(legacy));
}

inline LAYER_BLEND_TYPE toLegacyBlendType(const BlendMode mode) {
    return static_cast<LAYER_BLEND_TYPE>(static_cast<int>(mode));
}

/**
 * @brief Helper class for BlendMode information
 */
class BlendModeUtils {
public:
    static QString toString(BlendMode mode) {
        switch (mode) {
            case BlendMode::Normal: return "Normal";
            case BlendMode::Add: return "Add";
            case BlendMode::Subtract: return "Subtract";
            case BlendMode::Multiply: return "Multiply";
            case BlendMode::Screen: return "Screen";
            case BlendMode::Overlay: return "Overlay";
            case BlendMode::Darken: return "Darken";
            case BlendMode::Lighten: return "Lighten";
            case BlendMode::ColorDodge: return "Color Dodge";
            case BlendMode::ColorBurn: return "Color Burn";
            case BlendMode::HardLight: return "Hard Light";
            case BlendMode::SoftLight: return "Soft Light";
            case BlendMode::Difference: return "Difference";
            case BlendMode::Exclusion: return "Exclusion";
            case BlendMode::Hue: return "Hue";
            case BlendMode::Saturation: return "Saturation";
            case BlendMode::Color: return "Color";
            case BlendMode::Luminosity: return "Luminosity";
            case BlendMode::LinearBurn: return "Linear Burn";
            case BlendMode::Divide: return "Divide";
            case BlendMode::PinLight: return "Pin Light";
            case BlendMode::VividLight: return "Vivid Light";
            case BlendMode::LinearLight: return "Linear Light";
            case BlendMode::HardMix: return "Hard Mix";
            case BlendMode::Dissolve: return "Dissolve";
            case BlendMode::DancingDissolve: return "Dancing Dissolve";
            case BlendMode::ClassicColorBurn: return "Classic Color Burn";
            case BlendMode::LinearDodge: return "Linear Dodge";
            case BlendMode::ClassicColorDodge: return "Classic Color Dodge";
            case BlendMode::ClassicDifference: return "Classic Difference";
            case BlendMode::StencilAlpha: return "Stencil Alpha";
            case BlendMode::StencilLuma: return "Stencil Luma";
            case BlendMode::SilhouetteAlpha: return "Silhouette Alpha";
            case BlendMode::SilhouetteLuma: return "Silhouette Luma";
            default: return "Unknown";
        }
    }

    static BlendMode fromString(const QString& str) {
        QString s = str.toLower().trimmed().replace(" ", "");
        if (s == "normal") return BlendMode::Normal;
        if (s == "add") return BlendMode::Add;
        if (s == "subtract") return BlendMode::Subtract;
        if (s == "multiply") return BlendMode::Multiply;
        if (s == "screen") return BlendMode::Screen;
        if (s == "overlay") return BlendMode::Overlay;
        if (s == "darken") return BlendMode::Darken;
        if (s == "lighten") return BlendMode::Lighten;
        if (s == "colordodge") return BlendMode::ColorDodge;
        if (s == "colorburn") return BlendMode::ColorBurn;
        if (s == "hardlight") return BlendMode::HardLight;
        if (s == "softlight") return BlendMode::SoftLight;
        if (s == "difference") return BlendMode::Difference;
        if (s == "exclusion") return BlendMode::Exclusion;
        if (s == "hue") return BlendMode::Hue;
        if (s == "saturation") return BlendMode::Saturation;
        if (s == "color") return BlendMode::Color;
        if (s == "luminosity") return BlendMode::Luminosity;
        if (s == "linearburn") return BlendMode::LinearBurn;
        if (s == "divide") return BlendMode::Divide;
        if (s == "pinlight") return BlendMode::PinLight;
        if (s == "vividlight") return BlendMode::VividLight;
        if (s == "linearlight") return BlendMode::LinearLight;
        if (s == "hardmix") return BlendMode::HardMix;
        if (s == "dissolve") return BlendMode::Dissolve;
        if (s == "dancingdissolve") return BlendMode::DancingDissolve;
        if (s == "classiccolorburn") return BlendMode::ClassicColorBurn;
        if (s == "lineardodge") return BlendMode::LinearDodge;
        if (s == "classiccolordodge") return BlendMode::ClassicColorDodge;
        if (s == "classicdifference") return BlendMode::ClassicDifference;
        if (s == "stencilalpha") return BlendMode::StencilAlpha;
        if (s == "stencilluma") return BlendMode::StencilLuma;
        if (s == "silhouettealpha") return BlendMode::SilhouetteAlpha;
        if (s == "silhouetteluma") return BlendMode::SilhouetteLuma;
        return BlendMode::Normal;
    }
};

} // namespace ArtifactCore
