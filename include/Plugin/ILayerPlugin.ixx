module;

#include <memory>
#include <string>
#include <vector>
#include <QJsonObject>
#include <QSize>

export module ArtifactCore.Plugin.Layer.Interface;

import ArtifactCore.Plugin.Common;
import Property.Abstract;

export namespace ArtifactCore {

struct DrawContext {
    float currentTime = 0.0f;
    int frameNumber = 0;
    int compositionWidth = 0;
    int compositionHeight = 0;
};

class ILayerPlugin {
public:
    virtual ~ILayerPlugin() = default;

    virtual std::string pluginId() const = 0;
    virtual std::string displayName() const = 0;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual std::vector<PropertyGroup> extraPropertyGroups() = 0;

    virtual void drawContent(void* layerPtr,
                             const DrawContext& ctx) = 0;

    virtual void serializeExtra(void* layerPtr,
                                QJsonObject& json) = 0;
    virtual void deserializeExtra(void* layerPtr,
                                  const QJsonObject& json) = 0;
};

} // namespace ArtifactCore
