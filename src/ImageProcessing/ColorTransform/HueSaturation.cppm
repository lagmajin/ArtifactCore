module;

#include <algorithm>
#include <cmath>
#include <vector>
#include <QImage>
#include <QtMath>
#include <QColor>

module ImageProcessing.ColorTransform.HueSaturation;

import ImageProcessing.ColorTransform.HueSaturation;

namespace ArtifactCore {

// ============================================================================
// HueSaturationSettings 実装
// ============================================================================

HueSaturationSettings HueSaturationSettings::increaseSaturation() {
    HueSaturationSettings settings;
    settings.masterSaturation = 20.0;
    return settings;
}

HueSaturationSettings HueSaturationSettings::decreaseSaturation() {
    HueSaturationSettings settings;
    settings.masterSaturation = -20.0;
    return settings;
}

HueSaturationSettings HueSaturationSettings::colorize(double hue, double saturation) {
    HueSaturationSettings settings;
    settings.masterHue = hue;
    settings.masterSaturation = saturation;
    settings.masterLightness = -20.0;  // 少し暗くして色付け効果を強める
    return settings;
}

// ============================================================================
// HueSaturationEffect 実装
// ============================================================================

class HueSaturationEffect::Impl {
public:
    HueSaturationSettings settings_;
    
    // チャンネル範囲定義 (色相角度に基づく)
    struct ChannelRange {
        float minHue;
        float maxHue;
        double hueAdj;
        double satAdj;
        double lightAdj;
    };
    
    std::vector<ChannelRange> channelRanges_;
    
    Impl() {
        // チャンネル範囲を初期化 (AfterEffects互換)
        // Red: -30° to 30° (330° to 30°)
        channelRanges_.push_back({330.0f/360.0f, 30.0f/360.0f, 0,0,0});
        // Yellow: 30° to 90°
        channelRanges_.push_back({30.0f/360.0f, 90.0f/360.0f, 0,0,0});
        // Green: 90° to 150°
        channelRanges_.push_back({90.0f/360.0f, 150.0f/360.0f, 0,0,0});
        // Cyan: 150° to 210°
        channelRanges_.push_back({150.0f/360.0f, 210.0f/360.0f, 0,0,0});
        // Blue: 210° to 270°
        channelRanges_.push_back({210.0f/360.0f, 270.0f/360.0f, 0,0,0});
        // Magenta: 270° to 330°
        channelRanges_.push_back({270.0f/360.0f, 330.0f/360.0f, 0,0,0});
    }
    
    void updateChannelRanges(const HueSaturationSettings& settings) {
        channelRanges_[0].hueAdj = settings.redHue;
        channelRanges_[0].satAdj = settings.redSaturation;
        channelRanges_[0].lightAdj = settings.redLightness;
        
        channelRanges_[1].hueAdj = settings.yellowHue;
        channelRanges_[1].satAdj = settings.yellowSaturation;
        channelRanges_[1].lightAdj = settings.yellowLightness;
        
        channelRanges_[2].hueAdj = settings.greenHue;
        channelRanges_[2].satAdj = settings.greenSaturation;
        channelRanges_[2].lightAdj = settings.greenLightness;
        
        channelRanges_[3].hueAdj = settings.cyanHue;
        channelRanges_[3].satAdj = settings.cyanSaturation;
        channelRanges_[3].lightAdj = settings.cyanLightness;
        
        channelRanges_[4].hueAdj = settings.blueHue;
        channelRanges_[4].satAdj = settings.blueSaturation;
        channelRanges_[4].lightAdj = settings.blueLightness;
        
        channelRanges_[5].hueAdj = settings.magentaHue;
        channelRanges_[5].satAdj = settings.magentaSaturation;
        channelRanges_[5].lightAdj = settings.magentaLightness;
    }
    
    ChannelRange getChannelForHue(float hue) const {
        // Red channel wraps around 0/360
        if (hue >= channelRanges_[0].minHue || hue <= channelRanges_[0].maxHue) {
            return channelRanges_[0];
        }
        
        for (const auto& range : channelRanges_) {
            if (hue >= range.minHue && hue <= range.maxHue) {
                return range;
            }
        }
        
        // Default to master (no channel adjustment)
        return {0,0,0,0,0};
    }
};

HueSaturationEffect::HueSaturationEffect() : impl_(new Impl()) {}

HueSaturationEffect::~HueSaturationEffect() {
    delete impl_;
}

void HueSaturationEffect::setSettings(const HueSaturationSettings& settings) {
    impl_->settings_ = settings;
    impl_->updateChannelRanges(settings);
}

HueSaturationSettings HueSaturationEffect::settings() const {
    return impl_->settings_;
}

QImage HueSaturationEffect::apply(const QImage& source) const {
    QImage result = source.convertToFormat(QImage::Format_RGB32);
    
    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            QColor color(line[x]);
            float r = color.redF();
            float g = color.greenF();
            float b = color.blueF();
            
            applyPixel(r, g, b);
            
            color.setRgbF(r, g, b);
            line[x] = color.rgb();
        }
    }
    
    return result;
}

void HueSaturationEffect::applyPixel(float& r, float& g, float& b) const {
    float h, s, l;
    rgbToHsl(r, g, b, h, s, l);
    
    // マスター調整
    h = adjustHue(h, impl_->settings_.masterHue / 360.0f);
    s = adjustSaturation(s, impl_->settings_.masterSaturation / 100.0f);
    l = adjustLightness(l, impl_->settings_.masterLightness / 100.0f);
    
    // チャンネル別調整
    applyChannelAdjustment(h, s, l);
    
    hslToRgb(h, s, l, r, g, b);
}

void HueSaturationEffect::rgbToHsl(float r, float g, float b, float& h, float& s, float& l) {
    float max = std::max({r, g, b});
    float min = std::min({r, g, b});
    l = (max + min) / 2.0f;
    
    if (max == min) {
        h = s = 0.0f; // achromatic
    } else {
        float delta = max - min;
        s = l > 0.5f ? delta / (2.0f - max - min) : delta / (max + min);
        
        if (max == r) {
            h = (g - b) / delta + (g < b ? 6.0f : 0.0f);
        } else if (max == g) {
            h = (b - r) / delta + 2.0f;
        } else {
            h = (r - g) / delta + 4.0f;
        }
        h /= 6.0f;
    }
}

void HueSaturationEffect::hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
    if (s == 0.0f) {
        r = g = b = l; // achromatic
    } else {
        auto hue2rgb = [](float p, float q, float t) {
            if (t < 0.0f) t += 1.0f;
            if (t > 1.0f) t -= 1.0f;
            if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
            if (t < 1.0f/2.0f) return q;
            if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
            return p;
        };
        
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = hue2rgb(p, q, h + 1.0f/3.0f);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0f/3.0f);
    }
}

float HueSaturationEffect::adjustHue(float hue, float adjustment) {
    hue += adjustment;
    while (hue < 0.0f) hue += 1.0f;
    while (hue > 1.0f) hue -= 1.0f;
    return hue;
}

float HueSaturationEffect::adjustSaturation(float saturation, float adjustment) {
    saturation *= (1.0f + adjustment);
    return std::clamp(saturation, 0.0f, 1.0f);
}

float HueSaturationEffect::adjustLightness(float lightness, float adjustment) {
    lightness += adjustment;
    return std::clamp(lightness, 0.0f, 1.0f);
}

void HueSaturationEffect::applyChannelAdjustment(float& h, float& s, float& l) const {
    auto channel = impl_->getChannelForHue(h);
    
    if (channel.hueAdj != 0.0 || channel.satAdj != 0.0 || channel.lightAdj != 0.0) {
        h = adjustHue(h, channel.hueAdj / 360.0f);
        s = adjustSaturation(s, channel.satAdj / 100.0f);
        l = adjustLightness(l, channel.lightAdj / 100.0f);
    }
}

} // namespace ArtifactCore