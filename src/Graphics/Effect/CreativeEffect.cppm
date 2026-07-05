module;
#include <utility>
#include <string>
#include <vector>
#include <variant>
#include <algorithm>

module Graphics.Effect.Creative;

namespace ArtifactCore {

void CreativeEffect::setParameter(const std::string& name, float value) {
    auto it = std::find_if(parameters_.begin(), parameters_.end(), 
        [&](const EffectParameter& p) { return p.name == name; });
        
    if (it != parameters_.end()) {
        it->value = value;
    }
}

float CreativeEffect::getParameter(const std::string& name) const {
    auto it = std::find_if(parameters_.begin(), parameters_.end(), 
        [&](const EffectParameter& p) { return p.name == name; });
        
    if (it != parameters_.end()) {
        if (std::holds_alternative<float>(it->value)) {
            return std::get<float>(it->value);
        }
    }
    return 0.0f;
}

} // namespace ArtifactCore
