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

inline MirrorEffect::MirrorEffect() {
    parameters_.push_back({"Angle", "Axis Angle", EffectParameterType::Float, 0.0f, -3.1415f, 3.1415f});
    parameters_.push_back({"CenterX", "Center X", EffectParameterType::Float, 0.5f, 0.0f, 1.0f});
    parameters_.push_back({"CenterY", "Center Y", EffectParameterType::Float, 0.5f, 0.0f, 1.0f});
}

inline void MirrorEffect::process(VideoFrame& frame, const CreativeEffectContext&) {
    if (!enabled_) return;

    auto r_ch = frame.getChannel(ChannelType::Red);
    auto g_ch = frame.getChannel(ChannelType::Green);
    auto b_ch = frame.getChannel(ChannelType::Blue);
    if (!r_ch || !g_ch || !b_ch) return;

    const int w = frame.width();
    const int h = frame.height();

    std::vector<float> r_old(r_ch->data(), r_ch->data() + r_ch->size());
    std::vector<float> g_old(g_ch->data(), g_ch->data() + g_ch->size());
    std::vector<float> b_old(b_ch->data(), b_ch->data() + b_ch->size());

    const float cx = getParameter("CenterX") * w;
    const float cy = getParameter("CenterY") * h;
    const float ang = angle();
    const float nx = std::cos(ang);
    const float ny = std::sin(ang);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float dx = x - cx;
            const float dy = y - cy;
            const float dist = dx * nx + dy * ny;

            if (dist > 0) {
                const float sx = x - 2.0f * dist * nx;
                const float sy = y - 2.0f * dist * ny;

                const int isx = std::clamp((int)sx, 0, w - 1);
                const int isy = std::clamp((int)sy, 0, h - 1);
                const int idx = y * w + x;
                const int sidx = isy * w + isx;

                r_ch->data()[idx] = r_old[sidx];
                g_ch->data()[idx] = g_old[sidx];
                b_ch->data()[idx] = b_old[sidx];
            }
        }
    }
}

} // namespace ArtifactCore
