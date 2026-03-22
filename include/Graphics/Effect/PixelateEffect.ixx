module;
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.Pixelate;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

/**
 * @brief ピクセル化エフェクト
 * 画素をブロック状にまとめ、レトロなドット絵風の質感を生成します。
 */
class LIBRARY_DLL_API PixelateEffect : public CreativeEffect {
public:
    PixelateEffect();
    virtual ~PixelateEffect() = default;

    std::string getName() const override { return "Pixelate"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float blockSize() const { return getParameter("BlockSize"); }
};

} // namespace ArtifactCore
