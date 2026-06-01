module;
#include <cstddef>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.LightPressure;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

/**
 * @brief 明るい部分が画面に圧力をかけて歪ませるようなエフェクト
 */
class LIBRARY_DLL_API LightPressureEffect : public CreativeEffect {
public:
    LightPressureEffect();
    virtual ~LightPressureEffect() = default;

    std::string getName() const override { return "Light Pressure"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float pressure() const { return getParameter("Pressure"); }
    float bloom() const { return getParameter("Bloom"); }
    float spread() const { return getParameter("Spread"); }
    float compression() const { return getParameter("Compression"); }
};

} // namespace ArtifactCore
