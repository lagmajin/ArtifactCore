module;
#include <utility>
#include <vector>
#include <memory>
#include <algorithm>

module Graphics.Effect.Creative.Manager;

import Graphics.Effect.Creative;
import Channel;

namespace ArtifactCore {

CreativeEffectManager::CreativeEffectManager() {}
CreativeEffectManager::~CreativeEffectManager() = default;

void CreativeEffectManager::addEffect(std::shared_ptr<CreativeEffect> effect) {
    if (effect) {
        effectStack_.push_back(effect);
    }
}

void CreativeEffectManager::removeEffect(int index) {
    if (index >= 0 && index < effectStack_.size()) {
        effectStack_.erase(effectStack_.begin() + index);
    }
}

void CreativeEffectManager::applyAll(VideoFrame& frame, const CreativeEffectContext& context) {
    for (auto& effect : effectStack_) {
        if (effect && effect->isEnabled()) {
            effect->process(frame, context);
        }
    }
}

} // namespace ArtifactCore
