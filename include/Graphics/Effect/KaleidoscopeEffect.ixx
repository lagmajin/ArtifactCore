module;
#include <utility>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"
#include <cmath>
#include <algorithm>

export module Graphics.Effect.Creative.Kaleidoscope;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

/**
 * @brief 万華鏡エフェクト
 * 画像を扇状に分割し、回転・反転させて対称なパターンを生成します。
 */
class LIBRARY_DLL_API KaleidoscopeEffect : public CreativeEffect {
public:
    KaleidoscopeEffect();
    virtual ~KaleidoscopeEffect() = default;

    std::string getName() const override { return "Kaleidoscope"; }
    std::string getCategory() const override { return "Distort"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float count() const { return getParameter("Count"); }
    float angle() const { return getParameter("Angle"); }
    float centerX() const { return getParameter("CenterX"); }
    float centerY() const { return getParameter("CenterY"); }
};


} // namespace ArtifactCore
