module;
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