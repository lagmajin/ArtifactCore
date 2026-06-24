module;
#include <utility>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"
#include <cmath>
#include <algorithm>

export module Graphics.Effect.Creative.Mirror;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

/**
 * @brief ミラー（鏡像）エフェクト
 * 指定した軸を境に画像を反転させ、対称なレイアウトを作ります。
 */
class LIBRARY_DLL_API MirrorEffect : public CreativeEffect {
public:
    MirrorEffect();
    virtual ~MirrorEffect() = default;

    std::string getName() const override { return "Mirror"; }
    std::string getCategory() const override { return "Distort"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float angle() const { return getParameter("Angle"); }
    float offset() const { return getParameter("Offset"); }
};


} // namespace ArtifactCore
