module;

#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <cmath>

module ImageProcessing;

namespace ArtifactCore {

// Default parameter list: empty (subclasses override)
std::vector<EffectParamDef> AbstractImageEffect::parameters() const {
    return {};
}

void AbstractImageEffect::setParam(const std::string& name, double value) {
    size_t idx = 0;
    if (findParamIndex(name, idx)) {
        const auto defs = parameters();
        if (idx >= defs.size() || !std::isfinite(value)) return;
        const auto& definition = defs[idx];
        const double lower = std::min(definition.minValue, definition.maxValue);
        const double upper = std::max(definition.minValue, definition.maxValue);
        paramValues_[idx].second = std::clamp(value, lower, upper);
    }
}

double AbstractImageEffect::getParam(const std::string& name) const {
    size_t idx = 0;
    if (findParamIndex(name, idx)) {
        return paramValues_[idx].second;
    }
    return 0.0;
}

EffectROI AbstractImageEffect::roiHint() const {
    return {};
}

void AbstractImageEffect::chainProcess(ImageF32x4_RGBA& image) {
    process(image);
    if (next_) {
        next_->chainProcess(image);
    }
}

bool AbstractImageEffect::findParamIndex(const std::string& name, size_t& idx) const {
    const auto& params = const_cast<AbstractImageEffect*>(this)->parameters();
    if (params.empty()) return false;
    if (paramValues_.size() != params.size()) {
        auto* self = const_cast<AbstractImageEffect*>(this);
        self->paramValues_.resize(params.size());
        for (size_t i = 0; i < params.size(); ++i) {
            self->paramValues_[i].first = params[i].name;
            self->paramValues_[i].second = params[i].defaultValue;
        }
    }
    for (size_t i = 0; i < paramValues_.size(); ++i) {
        if (paramValues_[i].first == name) {
            idx = i;
            return true;
        }
    }
    return false;
}

} // namespace ArtifactCore
