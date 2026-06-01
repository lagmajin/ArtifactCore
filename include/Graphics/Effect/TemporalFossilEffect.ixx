module;
#include <utility>
#include <cstddef>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.TemporalFossil;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

class LIBRARY_DLL_API TemporalFossilEffect : public CreativeEffect {
public:
    TemporalFossilEffect();
    virtual ~TemporalFossilEffect() = default;

    std::string getName() const override { return "Temporal Fossil"; }
    std::string getCategory() const override { return "Temporal Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float persistence() const { return getParameter("Persistence"); }
    float decay() const { return getParameter("Decay"); }
    float edgeThreshold() const { return getParameter("EdgeThreshold"); }
    float chromaEcho() const { return getParameter("ChromaEcho"); }

    void ensureHistorySize(int width, int height);
    void resetHistoryFromSource(const float* r, const float* g, const float* b, std::size_t count);

    std::vector<float> previousR_;
    std::vector<float> previousG_;
    std::vector<float> previousB_;
    std::vector<float> fossilR_;
    std::vector<float> fossilG_;
    std::vector<float> fossilB_;
    int historyWidth_ = 0;
    int historyHeight_ = 0;
    int lastFrameIndex_ = -1;
};

} // namespace ArtifactCore
