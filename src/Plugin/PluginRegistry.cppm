module;
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

module ArtifactCore.Plugin.Registry;

import ArtifactCore.Plugin.Common;

namespace ArtifactCore {

ArtifactPluginRegistry& ArtifactPluginRegistry::instance() {
    static ArtifactPluginRegistry registry;
    return registry;
}

void ArtifactPluginRegistry::registerPlugin(const PluginDescriptor& descriptor) {
    std::lock_guard<std::mutex> lock(mutex_);
    Entry entry;
    entry.descriptor = descriptor;
    entry.descriptor.state = PluginState::Registered;
    entries_[descriptor.id] = std::move(entry);
}

void ArtifactPluginRegistry::unregisterPlugin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_.erase(id);
    auto it = entries_.find(id);
    if (it != entries_.end()) {
        it->second.descriptor.state = PluginState::Unloaded;
    }
}

void ArtifactPluginRegistry::activatePlugin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end()) {
        it->second.descriptor.state = PluginState::Active;
        active_.insert(id);
    }
}

void ArtifactPluginRegistry::deactivatePlugin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_.erase(id);
    auto it = entries_.find(id);
    if (it != entries_.end() && it->second.descriptor.state == PluginState::Active) {
        it->second.descriptor.state = PluginState::Inactive;
    }
}

bool ArtifactPluginRegistry::isRegistered(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.find(id) != entries_.end();
}

bool ArtifactPluginRegistry::isActive(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_.find(id) != active_.end();
}

PluginState ArtifactPluginRegistry::pluginState(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end()) {
        return it->second.descriptor.state;
    }
    return PluginState::Discovered;
}

std::vector<PluginDescriptor> ArtifactPluginRegistry::pluginsOfCategory(PluginCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PluginDescriptor> result;
    for (const auto& [id, entry] : entries_) {
        if (entry.descriptor.category == category) {
            result.push_back(entry.descriptor);
        }
    }
    return result;
}

std::optional<PluginDescriptor> ArtifactPluginRegistry::pluginById(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end()) {
        return it->second.descriptor;
    }
    return std::nullopt;
}

std::vector<PluginDescriptor> ArtifactPluginRegistry::allPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PluginDescriptor> result;
    for (const auto& [id, entry] : entries_) {
        result.push_back(entry.descriptor);
    }
    return result;
}

std::vector<PluginDescriptor> ArtifactPluginRegistry::activePlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PluginDescriptor> result;
    for (const auto& id : active_) {
        auto it = entries_.find(id);
        if (it != entries_.end()) {
            result.push_back(it->second.descriptor);
        }
    }
    return result;
}

} // namespace ArtifactCore
