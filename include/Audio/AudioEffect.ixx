module;
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect;

import Audio.Segment;
import Core.ArtifactString;

export namespace ArtifactCore {

struct EffectParameter {
    ArtifactCore::String id;
    ArtifactCore::String displayName;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    float value = 0.0f;
};

class LIBRARY_DLL_API AudioEffect {
public:
    virtual ~AudioEffect() = default;

    virtual ArtifactCore::String getName() const = 0;

    virtual void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) = 0;

    virtual void setBypass(bool bypass) { bypass_ = bypass; }
    virtual bool isBypassed() const { return bypass_; }

    // Parameter introspection & control
    virtual std::vector<EffectParameter> getParameters() const { return {}; }
    virtual void setParameterValue(const ArtifactCore::String& /*id*/, float /*value*/) {}
    virtual float getParameterValue(const ArtifactCore::String& id) const {
        for (auto& p : getParameters()) {
            if (p.id == id) return p.value;
        }
        return 0.0f;
    }

    // Factory ID used for serialization (e.g. "compressor", "delay", "reverb")
    virtual ArtifactCore::String effectType() const { return ArtifactCore::String("unknown"); }

    // Serialization
    virtual QJsonObject toJson() const {
        QJsonObject obj;
        obj["type"] = QString::fromUtf8(
            ArtifactCore::toStdString(effectType()).data());
        obj["bypass"] = bypass_;
        return obj;
    }
    virtual void fromJson(const QJsonObject& obj) {
        bypass_ = obj["bypass"].toBool(false);
    }

protected:
    bool bypass_ = false;
};

} // namespace ArtifactCore
