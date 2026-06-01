module;
#include <utility>
#include <cstddef>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.SurfaceMemory;

import Graphics.Effect.Creative;
import Channel;
import Math.Noise;

export namespace ArtifactCore {

class LIBRARY_DLL_API SurfaceMemoryEffect : public CreativeEffect {
public:
    SurfaceMemoryEffect();
    virtual ~SurfaceMemoryEffect() = default;

    std::string getName() const override { return "Surface Memory"; }
    std::string getCategory() const override { return "Temporal Stylize"; }

    void process(VideoFrame& frame, const CreativeEffectContext& context) override;

private:
    float retention() const { return getParameter("Retention"); }
    float refresh() const { return getParameter("Refresh"); }
    float textureAmount() const { return getParameter("Texture"); }
    float smear() const { return getParameter("Smear"); }

    void ensureMemorySize(int width, int height);
    void resetMemoryFromSource(const float* r, const float* g, const float* b, std::size_t count);

    std::vector<float> memoryR_;
    std::vector<float> memoryG_;
    std::vector<float> memoryB_;
    int memoryWidth_ = 0;
    int memoryHeight_ = 0;
    int lastFrameIndex_ = -1;
};

} // namespace ArtifactCore
