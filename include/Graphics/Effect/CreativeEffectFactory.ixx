module;
#include <utility>
#include <vector>
#include <memory>
#include <QDebug>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.Factory;

import Graphics.Effect.Creative;
import Core.ArtifactString;
import Memory.SharedPtr;
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
    static SharedPtr<CreativeEffect> create(const String& name);
    static std::vector<String> getAvailableEffects();
};

inline SharedPtr<CreativeEffect> CreativeEffectFactory::create(const String& name) {
    if (name == "Glitch") return makeShared<GlitchCreativeEffect>();
    if (name == "Kaleidoscope") return makeShared<KaleidoscopeEffect>();
    if (name == "Mirror") return makeShared<MirrorEffect>();
    if (name == "Halftone") return makeShared<HalftoneEffect>();
    if (name == "Pixelate") return makeShared<PixelateEffect>();
    if (name == "Posterize") return makeShared<PosterizeEffect>();
    if (name == "Fisheye") return makeShared<FisheyeEffect>();
    if (name == "DepthMelt") return makeShared<DepthMeltEffect>();
    if (name == "EdgeEcho") return makeShared<EdgeEchoEffect>();
    if (name == "LightPressure") return makeShared<LightPressureEffect>();
    if (name == "TemporalFossil") return makeShared<TemporalFossilEffect>();
    if (name == "PigmentSeparation") return makeShared<PigmentSeparationEffect>();
    if (name == "SurfaceMemory") return makeShared<SurfaceMemoryEffect>();
    if (name == "Emboss") return makeShared<EmbossEffect>();
    if (name == "Solarize") return makeShared<SolarizeEffect>();
    if (name == "ChromaticAberration") return makeShared<ChromaticAberrationEffect>();
    if (name == "OldTV" || name == "Old TV / CRT") return makeShared<OldTVEffect>();
    if (name == "ColorVibrance" || name == "VC Color Vibrance") return makeShared<ColorVibranceEffect>();

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

inline std::vector<String> CreativeEffectFactory::getAvailableEffects() {
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
