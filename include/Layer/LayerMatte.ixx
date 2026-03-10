module;
#include <QString>

export module Layer.Matte;

export namespace ArtifactCore {

/**
 * @brief Track matte modes for using one layer as a mask for another.
 */
enum class MatteMode {
    None,
    Alpha,           // Use Alpha channel of the matte layer
    AlphaInverted,   // Use Inverted Alpha channel
    Luminance,       // Use Luminance (brightness) of the matte layer
    LuminanceInverted // Use Inverted Luminance
};

/**
 * @brief Helper class for MatteMode information
 */
class MatteModeUtils {
public:
    static QString toString(MatteMode mode) {
        switch (mode) {
            case MatteMode::None: return "None";
            case MatteMode::Alpha: return "Alpha Matte";
            case MatteMode::AlphaInverted: return "Alpha Inverted Matte";
            case MatteMode::Luminance: return "Luma Matte";
            case MatteMode::LuminanceInverted: return "Luma Inverted Matte";
            default: return "Unknown";
        }
    }

    static MatteMode fromString(const QString& str) {
        QString s = str.toLower().trimmed().replace(" ", "");
        if (s == "none") return MatteMode::None;
        if (s == "alpha" || s == "alphamatte") return MatteMode::Alpha;
        if (s == "alphainverted" || s == "alphainvertedmatte") return MatteMode::AlphaInverted;
        if (s == "luminance" || s == "lumamatte") return MatteMode::Luminance;
        if (s == "luminanceinverted" || s == "lumainvertedmatte") return MatteMode::LuminanceInverted;
        return MatteMode::None;
    }
};

} // namespace ArtifactCore