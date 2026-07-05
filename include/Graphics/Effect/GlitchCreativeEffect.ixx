module;
#include <utility>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.Glitch;

import Graphics.Effect.Creative;
import Channel;
import Math.Noise;

export namespace ArtifactCore {

/**
 * @brief スタイリッシュなグリッチエフェクト
 * 画素のズレ、チャンネルの分離、ノイズの混入を再現します。
 */
class LIBRARY_DLL_API GlitchCreativeEffect : public CreativeEffect {
public:
    GlitchCreativeEffect();
    virtual ~GlitchCreativeEffect() = default;

    std::string getName() const override { return "Stylish Glitch"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    // パラメータアクセスのヘルパー
    float glitchAmount() const { return getParameter("Amount"); }
    float rgbSplit() const { return getParameter("RGBSplit"); }
    float blockNoise() const { return getParameter("BlockNoise"); }
};

} // namespace ArtifactCore
