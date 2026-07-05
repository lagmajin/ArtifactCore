module;
#include <utility>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.Fisheye;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

/**
 * @brief 魚眼レンズ（Fisheye）エフェクト
 * 広角レンズ特有の樽型歪みをシミュレートします。
 */
class LIBRARY_DLL_API FisheyeEffect : public CreativeEffect {
public:
    FisheyeEffect();
    virtual ~FisheyeEffect() = default;

    std::string getName() const override { return "Fisheye Lens"; }
    std::string getCategory() const override { return "Distort"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float strength() const { return getParameter("Strength"); }
    float zoom() const { return getParameter("Zoom"); }
};

} // namespace ArtifactCore
