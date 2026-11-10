module;
#include <mutex>
#include <string>
#include <utility>
#include <vector>

module ArtifactCore.Plugin.Registry;

import ArtifactCore.Plugin.Common;
import Core.ArtifactString;
import Core.ArtifactOptional;
import Container.NamedVector;

namespace ArtifactCore {

ArtifactPluginRegistry& ArtifactPluginRegistry::instance() {
    static ArtifactPluginRegistry registry;
    return registry;
}

void ArtifactPluginRegistry::registerPlugin(const PluginDescriptor& descriptor) {
    const std::string id = toStdString(descriptor.id);
    const int category = static_cast<int>(descriptor.category);
    if (id.empty() || category < static_cast<int>(PluginCategory::Effect) ||
        category > static_cast<int>(PluginCategory::ImportExport) ||
        descriptor.apiVersion <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    active_.erase(id);
    Entry entry;
    entry.descriptor = descriptor;
    entry.descriptor.state = PluginState::Registered;
    entries_[id] = std::move(entry);
}

void ArtifactPluginRegistry::unregisterPlugin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_.erase(id);
    if (Entry* const entry = entries_.findValue(id)) {
        entry->descriptor.state = PluginState::Unloaded;
    }
}

void ArtifactPluginRegistry::activatePlugin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (Entry* const entry = entries_.findValue(id);
        entry && (entry->descriptor.state == PluginState::Registered ||
                  entry->descriptor.state == PluginState::Inactive ||
                  entry->descriptor.state == PluginState::Validated)) {
        entry->descriptor.state = PluginState::Active;
        active_.insert(id);
    }
}

void ArtifactPluginRegistry::deactivatePlugin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_.erase(id);
    if (Entry* const entry = entries_.findValue(id);
        entry && entry->descriptor.state == PluginState::Active) {
        entry->descriptor.state = PluginState::Inactive;
    }
}

bool ArtifactPluginRegistry::isRegistered(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.contains(id);
}

bool ArtifactPluginRegistry::isActive(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_.contains(id);
}

PluginState ArtifactPluginRegistry::pluginState(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (const Entry* const entry = entries_.findValue(id)) return entry->descriptor.state;
    return PluginState::Discovered;
}

std::vector<PluginDescriptor> ArtifactPluginRegistry::pluginsOfCategory(PluginCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    NamedVector<PluginDescriptor> result;
    entries_.eachKeyValue([&](const std::string&, const Entry& entry) {
        if (entry.descriptor.category == category) {
            result.push_back(entry.descriptor);
        }
    });
    return result.toStdVector();
}

Optional<PluginDescriptor> ArtifactPluginRegistry::pluginById(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (const Entry* const entry = entries_.findValue(id)) return entry->descriptor;
    return {};
}

std::vector<PluginDescriptor> ArtifactPluginRegistry::allPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    NamedVector<PluginDescriptor> result;
    result.reserve(entries_.size());
    entries_.eachKeyValue([&](const std::string&, const Entry& entry) {
        result.push_back(entry.descriptor);
    });
    return result.toStdVector();
}

std::vector<PluginDescriptor> ArtifactPluginRegistry::activePlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    NamedVector<PluginDescriptor> result;
    result.reserve(active_.size());
    for (const auto& id : active_) {
        if (const Entry* const entry = entries_.findValue(id)) result.push_back(entry->descriptor);
    }
    return result.toStdVector();
}

} // namespace ArtifactCore
