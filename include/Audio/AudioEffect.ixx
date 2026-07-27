module;
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect;

import Audio.Segment;
export import Utils.Text.String;

export namespace ArtifactCore {

struct EffectParameter {
    String id;
    String displayName;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    float value = 0.0f;
};

class LIBRARY_DLL_API AudioEffect {
public:
    virtual ~AudioEffect() = default;

    virtual String getName() const = 0;

    virtual void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) = 0;

    virtual void setBypass(bool bypass) { bypass_ = bypass; }
    virtual bool isBypassed() const { return bypass_; }

    // Parameter introspection & control
    virtual std::vector<EffectParameter> getParameters() const { return {}; }
    virtual void setParameterValue(const String& /*id*/, float /*value*/) {}
    virtual float getParameterValue(const String& id) const {
        for (auto& p : getParameters()) {
            if (p.id == id) return p.value;
        }
        return 0.0f;
    }

    // Factory ID used for serialization (e.g. "compressor", "delay", "reverb")
    virtual String effectType() const { return "unknown"; }

    // Serialization
    virtual QJsonObject toJson() const {
        QJsonObject obj;
        const std::string type = toStdString(effectType());
        obj["type"] = QString::fromUtf8(type.data(), static_cast<int>(type.size()));
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
