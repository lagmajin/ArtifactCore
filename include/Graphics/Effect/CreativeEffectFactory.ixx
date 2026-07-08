module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <QDebug>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.Factory;

import Graphics.Effect.Creative;
import Core.Diagnostics.FallbackPolicy;
import Graphics.Effect.Creative.Emboss;
import Graphics.Effect.Creative.Fisheye;
import Graphics.Effect.Creative.Glitch;
import Graphics.Effect.Creative.Halftone;
import Graphics.Effect.Creative.ChromaticAberration;
import Graphics.Effect.Creative.Solarize;
import Graphics.Effect.Creative.DepthMelt;
import Graphics.Effect.Creative.EdgeEcho;
import Graphics.Effect.Creative.Kaleidoscope;
import Graphics.Effect.Creative.LightPressure;
import Graphics.Effect.Creative.Mirror;
import Graphics.Effect.Creative.PigmentSeparation;
import Graphics.Effect.Creative.ColorVibrance;
import Graphics.Effect.Creative.Pixelate;
import Graphics.Effect.Creative.Posterize;
import Graphics.Effect.Creative.SurfaceMemory;
import Graphics.Effect.Creative.TemporalFossil;
import Graphics.Effect.Creative.OldTV;

export namespace ArtifactCore {

/**
 * @brief 文字列名からクリエイティブエフェクトを生成するファクトリ
 */
class LIBRARY_DLL_API CreativeEffectFactory {
public:
    static std::shared_ptr<CreativeEffect> create(const std::string& name);
    static std::vector<std::string> getAvailableEffects();
};

inline std::shared_ptr<CreativeEffect> CreativeEffectFactory::create(const std::string& name) {
    if (name == "Glitch") return std::make_shared<GlitchCreativeEffect>();
    if (name == "Kaleidoscope") return std::make_shared<KaleidoscopeEffect>();
    if (name == "Mirror") return std::make_shared<MirrorEffect>();
    if (name == "Halftone") return std::make_shared<HalftoneEffect>();
    if (name == "Pixelate") return std::make_shared<PixelateEffect>();
    if (name == "Posterize") return std::make_shared<PosterizeEffect>();
    if (name == "Fisheye") return std::make_shared<FisheyeEffect>();
    if (name == "DepthMelt") return std::make_shared<DepthMeltEffect>();
    if (name == "EdgeEcho") return std::make_shared<EdgeEchoEffect>();
    if (name == "LightPressure") return std::make_shared<LightPressureEffect>();
    if (name == "TemporalFossil") return std::make_shared<TemporalFossilEffect>();
    if (name == "PigmentSeparation") return std::make_shared<PigmentSeparationEffect>();
    if (name == "SurfaceMemory") return std::make_shared<SurfaceMemoryEffect>();
    if (name == "Emboss") return std::make_shared<EmbossEffect>();
    if (name == "Solarize") return std::make_shared<SolarizeEffect>();
    if (name == "ChromaticAberration") return std::make_shared<ChromaticAberrationEffect>();
    if (name == "OldTV" || name == "Old TV / CRT") return std::make_shared<OldTVEffect>();
    if (name == "ColorVibrance" || name == "VC Color Vibrance") return std::make_shared<ColorVibranceEffect>();

    auto* tracker = FallbackTracker::instance();
    auto policy = tracker->policy(FallbackCategory::Effect);
    if (policy.action == FallbackAction::Bypass) {
        tracker->record(FallbackCategory::Effect, FallbackAction::Bypass,
                       QString::fromUtf8(name.data(), static_cast<int>(name.length())), "[effect bypassed]",
                       policy.warningMessage);
        qWarning() << "[CreativeEffectFactory] unsupported effect, bypassing:"
                   << QString::fromUtf8(name.data(), static_cast<int>(name.length()));
    } else {
        tracker->record(FallbackCategory::Effect, FallbackAction::Fallback,
                       QString::fromUtf8(name.data(), static_cast<int>(name.length())), "[null]",
                       "Unsupported effect, returning null");
        qWarning() << "[CreativeEffectFactory] unsupported effect:"
                   << QString::fromUtf8(name.data(), static_cast<int>(name.length()));
    }
    return nullptr;
 }

inline std::vector<std::string> CreativeEffectFactory::getAvailableEffects() {
    return {
        "Fisheye",
        "Glitch",
        "Halftone",
        "DepthMelt",
        "Kaleidoscope",
        "LightPressure",
        "OldTV",
        "Mirror",
        "EdgeEcho",
        "PigmentSeparation",
        "Pixelate",
        "Posterize",
        "SurfaceMemory",
        "TemporalFossil",
        "Emboss",
        "Solarize",
        "ChromaticAberration",
        "VC Color Vibrance"
    };
}

} // namespace ArtifactCore
