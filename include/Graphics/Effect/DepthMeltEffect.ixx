module;
#include <cstddef>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.DepthMelt;

import Graphics.Effect.Creative;
import Channel;
import Math.Noise;

export namespace ArtifactCore {

/**
 * @brief 擬似深度に応じて像が下方向へ溶け落ちるように見せるエフェクト
 */
class LIBRARY_DLL_API DepthMeltEffect : public CreativeEffect {
public:
    DepthMeltEffect();
    virtual ~DepthMeltEffect() = default;

    std::string getName() const override { return "Depth Melt"; }
    std::string getCategory() const override { return "Distort"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float meltAmount() const { return getParameter("Melt"); }
    float gravity() const { return getParameter("Gravity"); }
    float heat() const { return getParameter("Heat"); }
    float detail() const { return getParameter("Detail"); }
};

} // namespace ArtifactCore
