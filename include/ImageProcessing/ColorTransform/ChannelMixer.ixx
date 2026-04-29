module;
#include <algorithm>
#include <QImage>

export import Color.GamutConversion;

export module ImageProcessing.ColorTransform.ChannelMixer;

export namespace ArtifactCore {

struct ChannelMixerSettings {
    Matrix3x3 matrix = identity();
    float strength = 1.0f;
    bool monochrome = false;
    bool preserveLuma = false;

    void reset() {
        *this = ChannelMixerSettings{};
    }

    static ChannelMixerSettings identityMix();
    static ChannelMixerSettings warm();
    static ChannelMixerSettings cool();
    static ChannelMixerSettings crossProcess();
    static ChannelMixerSettings monochromeMix();
};

class ChannelMixerProcessor {
public:
    ChannelMixerProcessor();
    ~ChannelMixerProcessor();

    void setSettings(const ChannelMixerSettings& settings);
    const ChannelMixerSettings& settings() const;

    QImage apply(const QImage& source) const;
    void applyPixel(float& r, float& g, float& b) const;

private:
    ChannelMixerSettings settings_;

    static float luma(float r, float g, float b);
    static void preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled, float strength);
};

} // namespace ArtifactCore
