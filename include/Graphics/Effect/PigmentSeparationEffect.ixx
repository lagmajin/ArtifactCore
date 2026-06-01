module;
#include <utility>
#include <cstddef>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.PigmentSeparation;

import Graphics.Effect.Creative;
import Channel;
import Math.Noise;

export namespace ArtifactCore {

class LIBRARY_DLL_API PigmentSeparationEffect : public CreativeEffect {
public:
    PigmentSeparationEffect();
    virtual ~PigmentSeparationEffect() = default;

    std::string getName() const override { return "Pigment Separation"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float spread() const { return getParameter("Spread"); }
    float bleed() const { return getParameter("Bleed"); }
    float flow() const { return getParameter("Flow"); }
    float granulation() const { return getParameter("Granulation"); }
};

} // namespace ArtifactCore
