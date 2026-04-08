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
    Luminosity
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
    BLEND_LUMINOSITY = static_cast<int>(BlendMode::Luminosity)
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
        return BlendMode::Normal;
    }
};

} // namespace ArtifactCore
