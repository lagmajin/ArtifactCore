module;

#include <string>
#include <vector>

export module ArtifactCore.Plugin.Common;

import Core.ArtifactString;

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
    String id;
    String displayName;
    String version;
    String author;
    String description;
    PluginCategory category;
    int apiVersion = 1;
    String pluginPath;
    PluginState state = PluginState::Discovered;
};

enum class PluginLoadMode {
    DllInProcess,
    Subprocess,
    Auto
};

struct LoadResult {
    String pluginPath;
    String pluginId;
    bool success = false;
    String errorMessage;
    PluginLoadMode loadedMode = PluginLoadMode::Auto;
    String subprocessId;
};

} // namespace ArtifactCore
