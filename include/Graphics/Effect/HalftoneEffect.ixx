module;
#include <utility>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"
#include <cmath>
#include <algorithm>

export module Graphics.Effect.Creative.Halftone;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

/**
 * @brief ハーフトーン（網点）エフェクト
 * 画素の明るさをドットの大きさに変換し、新聞やポップアートのような質感を生成します。
 */
class LIBRARY_DLL_API HalftoneEffect : public CreativeEffect {
public:
    HalftoneEffect();
    virtual ~HalftoneEffect() = default;

    std::string getName() const override { return "Halftone"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float dotSize() const { return getParameter("Size"); }
    float dotAngle() const { return getParameter("Angle"); }
    float contrast() const { return getParameter("Contrast"); }
};


} // namespace ArtifactCore
