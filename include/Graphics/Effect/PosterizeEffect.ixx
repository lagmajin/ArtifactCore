module;
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.Posterize;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

/**
 * @brief ポスタリゼーションエフェクト
 * 画素の階調を減らし（量子化し）、色数を制限したポスターのような質感を生成します。
 */
class LIBRARY_DLL_API PosterizeEffect : public CreativeEffect {
public:
    PosterizeEffect();
    virtual ~PosterizeEffect() = default;

    std::string getName() const override { return "Posterize"; }
    std::string getCategory() const override { return "Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float levels() const { return getParameter("Levels"); }
};

} // namespace ArtifactCore
