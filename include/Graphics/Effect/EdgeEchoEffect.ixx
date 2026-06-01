module;
#include <cstddef>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.EdgeEcho;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

/**
 * @brief エッジの輪郭が少し遅れて重なるような残響を作るエフェクト
 */
class LIBRARY_DLL_API EdgeEchoEffect : public CreativeEffect {
public:
    EdgeEchoEffect();
    virtual ~EdgeEchoEffect() = default;

    std::string getName() const override { return "Edge Echo"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float intensity() const { return getParameter("Intensity"); }
    float thickness() const { return getParameter("Thickness"); }
    float echoCount() const { return getParameter("EchoCount"); }
    float tint() const { return getParameter("Tint"); }
};

} // namespace ArtifactCore
