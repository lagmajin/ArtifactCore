module;

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>

export module ArtifactCore.Plugin.Registry;

import ArtifactCore.Plugin.Common;

export namespace ArtifactCore {

class ArtifactPluginRegistry {
public:
    static ArtifactPluginRegistry& instance();

    void registerPlugin(const PluginDescriptor& descriptor);
    void unregisterPlugin(const std::string& id);

    void activatePlugin(const std::string& id);
    void deactivatePlugin(const std::string& id);

    bool isRegistered(const std::string& id) const;
    bool isActive(const std::string& id) const;
    PluginState pluginState(const std::string& id) const;

    std::vector<PluginDescriptor> pluginsOfCategory(PluginCategory category) const;
    std::optional<PluginDescriptor> pluginById(const std::string& id) const;
    std::vector<PluginDescriptor> allPlugins() const;
    std::vector<PluginDescriptor> activePlugins() const;

private:
    ArtifactPluginRegistry() = default;

    struct Entry {
        PluginDescriptor descriptor;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
    std::unordered_set<std::string> active_;
};

} // namespace ArtifactCore
