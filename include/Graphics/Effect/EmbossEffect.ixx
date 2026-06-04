module;
#include <utility>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"
#include <cmath>

export module Graphics.Effect.Creative.Emboss;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

class LIBRARY_DLL_API EmbossEffect : public CreativeEffect {
public:
    EmbossEffect();
    ~EmbossEffect() override = default;

    std::string getName() const override { return "Emboss"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float strength() const { return getParameter("Strength"); }
    float height() const { return getParameter("Height"); }
};

}
