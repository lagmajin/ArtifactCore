module;
#include <utility>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.ColorVibrance;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

class LIBRARY_DLL_API ColorVibranceEffect : public CreativeEffect {
public:
    ColorVibranceEffect();
    ~ColorVibranceEffect() override = default;

    std::string getName() const override { return "VC Color Vibrance"; }
    std::string getCategory() const override { return "Color Correction"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float vibrance() const { return getParameter("Vibrance"); }
    float saturation() const { return getParameter("Saturation"); }
    float colorBoost() const { return getParameter("ColorBoost"); }
    float matteAmount() const { return getParameter("MatteAmount"); }
    float matteThreshold() const { return getParameter("MatteThreshold"); }
    float matteSoftness() const { return getParameter("MatteSoftness"); }
};

} // namespace ArtifactCore
