module;
#include <utility>
#include <QString>
#include <vector>
export module Layer.Matte;

import FloatRGBA;
import Image.ImageF32x4_RGBA;
import Color.Luminance;


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

enum class MatteStackMode {
    Add,
    Common,
    Subtract
};

struct MatteEvaluationSettings {
    MatteMode mode = MatteMode::Alpha;
    MatteStackMode stackMode = MatteStackMode::Common;
    LuminanceStandard luminanceStandard = LuminanceStandard::Rec709;
    float opacity = 1.0f;
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

class LIBRARY_DLL_API MatteEvaluator {
public:
    static float sample(const FloatRGBA& pixel,
                        MatteMode mode,
                        LuminanceStandard standard = LuminanceStandard::Rec709);

    static float sample(const ImageF32x4_RGBA& image,
                        int x,
                        int y,
                        MatteMode mode,
                        LuminanceStandard standard = LuminanceStandard::Rec709);

    static float combine(float current, float next, MatteStackMode mode);

    static float evaluate(const std::vector<float>& matteFactors,
                          MatteStackMode stackMode = MatteStackMode::Common);

    static FloatRGBA apply(const FloatRGBA& source, float matteFactor);

    static ImageF32x4_RGBA apply(const ImageF32x4_RGBA& source,
                                 const ImageF32x4_RGBA& matte,
                                 const MatteEvaluationSettings& settings = {});
};

} // namespace ArtifactCore
