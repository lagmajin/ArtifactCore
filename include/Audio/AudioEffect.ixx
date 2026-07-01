module;
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect;

import Audio.Segment;

export namespace ArtifactCore {

struct EffectParameter {
    std::string id;
    std::string displayName;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    float value = 0.0f;
};

class LIBRARY_DLL_API AudioEffect {
public:
    virtual ~AudioEffect() = default;

    virtual std::string getName() const = 0;

    virtual void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) = 0;

    virtual void setBypass(bool bypass) { bypass_ = bypass; }
    virtual bool isBypassed() const { return bypass_; }

    // Parameter introspection & control
    virtual std::vector<EffectParameter> getParameters() const { return {}; }
    virtual void setParameterValue(const std::string& /*id*/, float /*value*/) {}
    virtual float getParameterValue(const std::string& id) const {
        for (auto& p : getParameters()) {
            if (p.id == id) return p.value;
        }
        return 0.0f;
    }

    // Factory ID used for serialization (e.g. "compressor", "delay", "reverb")
    virtual std::string effectType() const { return "unknown"; }

    // Serialization
    virtual QJsonObject toJson() const {
        QJsonObject obj;
        obj["type"] = QString::fromStdString(effectType());
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