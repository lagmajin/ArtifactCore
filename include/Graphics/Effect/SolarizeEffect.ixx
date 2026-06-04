module;
#include <utility>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"
#include <cmath>

export module Graphics.Effect.Creative.Solarize;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

class LIBRARY_DLL_API SolarizeEffect : public CreativeEffect {
public:
    SolarizeEffect();
    ~SolarizeEffect() override = default;

    std::string getName() const override { return "Solarize"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float threshold() const { return getParameter("Threshold"); }
};

}
