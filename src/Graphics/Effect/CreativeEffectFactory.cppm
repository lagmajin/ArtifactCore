module;
#include <utility>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <vector>

module Graphics.Effect.Creative.Factory;

import Graphics.Effect.Creative;
import Graphics.Effect.Creative.Glitch;
import Graphics.Effect.Creative.Kaleidoscope;
import Graphics.Effect.Creative.Mirror;
import Graphics.Effect.Creative.Halftone;
import Graphics.Effect.Creative.Pixelate;
import Graphics.Effect.Creative.Posterize;
import Graphics.Effect.Creative.Fisheye;

namespace ArtifactCore {

std::map<std::string, std::function<std::shared_ptr<CreativeEffect>()>> CreativeEffectFactory::registry_;

void CreativeEffectFactory::init() {
    if (!registry_.empty()) return;
    
    registry_["Glitch"] = []() { return std::make_shared<GlitchCreativeEffect>(); };
    registry_["Kaleidoscope"] = []() { return std::make_shared<KaleidoscopeEffect>(); };
    registry_["Mirror"] = []() { return std::make_shared<MirrorEffect>(); };
    registry_["Halftone"] = []() { return std::make_shared<HalftoneEffect>(); };
    registry_["Pixelate"] = []() { return std::make_shared<PixelateEffect>(); };
    registry_["Posterize"] = []() { return std::make_shared<PosterizeEffect>(); };
    registry_["Fisheye"] = []() { return std::make_shared<FisheyeEffect>(); };
    // 今後エフェクトが増えるたび、ここに追加します
}

std::shared_ptr<CreativeEffect> CreativeEffectFactory::create(const std::string& name) {
    init();
    if (registry_.find(name) != registry_.end()) {
        return registry_[name]();
    }
    return nullptr;
}

std::vector<std::string> CreativeEffectFactory::getAvailableEffects() {
    init();
    std::vector<std::string> names;
    for (auto const& [name, func] : registry_) {
        names.push_back(name);
    }
    return names;
}

} // namespace ArtifactCore
