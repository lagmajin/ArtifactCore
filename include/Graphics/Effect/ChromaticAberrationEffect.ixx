module;
#include <utility>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"
#include <cmath>

export module Graphics.Effect.Creative.ChromaticAberration;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

class LIBRARY_DLL_API ChromaticAberrationEffect : public CreativeEffect {
public:
    ChromaticAberrationEffect();
    ~ChromaticAberrationEffect() override = default;

    std::string getName() const override { return "Chromatic Aberration"; }
    std::string getCategory() const override { return "Distort"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float amount() const { return getParameter("Amount"); }
    float angle() const { return getParameter("Angle"); }
};

}
