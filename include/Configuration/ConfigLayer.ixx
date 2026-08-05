module;
#include <QString>

export module Configuration.ConfigLayer;

export namespace ArtifactCore {

enum class ConfigLayer : int {
    None = -1,
    System = 0,
    User = 1,
    Project = 2,
    Session = 3
};

constexpr bool isPersistentConfigLayer(ConfigLayer layer) noexcept {
    return layer != ConfigLayer::None && layer != ConfigLayer::Session;
}

constexpr int configLayerPriority(ConfigLayer layer) noexcept {
    return static_cast<int>(layer);
}

}
