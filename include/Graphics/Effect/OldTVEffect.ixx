module;
#include <utility>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.OldTV;

import Graphics.Effect.Creative;
import Channel;
import Math.Noise;

export namespace ArtifactCore {

/**
 * @brief オールドTV（ブラウン管）エフェクト
 * 走査線、画面の歪み、カラーフリンジ、ちらつきなどのノスタルジックな質感を生成します。
 */
class LIBRARY_DLL_API OldTVEffect : public CreativeEffect {
public:
    OldTVEffect();
    virtual ~OldTVEffect() = default;

    std::string getName() const override { return "Old TV / CRT"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float scanlineDensity() const { return getParameter("Scanline"); }
    float curvature() const { return getParameter("Curvature"); }
    float flicker() const { return getParameter("Flicker"); }
    float chromaticAberration() const { return getParameter("Fringe"); }
};

} // namespace ArtifactCore
