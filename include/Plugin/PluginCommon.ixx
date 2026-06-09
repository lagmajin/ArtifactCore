module;

#include <string>
#include <vector>
#include <optional>
#include <memory>

export module ArtifactCore.Plugin.Common;

export namespace ArtifactCore {

enum class PluginCategory {
    Effect = 0,
    Layer = 1,
    Tool = 2,
    ImportExport = 3
};

enum class PluginState {
    Discovered,
    Validated,
    Registered,
    Active,
    Inactive,
    Failed,
    Unloaded
};

struct PluginDescriptor {
    std::string id;
    std::string displayName;
    std::string version;
    std::string author;
    std::string description;
    PluginCategory category;
    int apiVersion = 1;
    std::string pluginPath;
    PluginState state = PluginState::Discovered;
};

enum class PluginLoadMode {
    DllInProcess,
    Subprocess,
    Auto
};

struct LoadResult {
    std::string pluginPath;
    std::string pluginId;
    bool success = false;
    std::string errorMessage;
    PluginLoadMode loadedMode = PluginLoadMode::Auto;
    std::string subprocessId;
};

} // namespace ArtifactCore
