module;
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
module ImageProcessing.ColorTransform.TriChromaticShift;

import Particle;
import FloatRGBA;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

// ============================================================
// 色空間変換（HSL との行き来 — TriChromaticShift の核）
// ============================================================

namespace {

constexpr double kPi = 3.14159265358979323846;

double hueToRgb(double p, double q, double t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0 / 2.0) return q;
    if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
}

} // anonymous namespace

void TriChromaticProcessor::hslToRgb(double h, double s, double l, double& r, double& g, double& b) {
    // h: 0-360, s: 0-1, l: 0-1
    double H = h / 360.0;
    double S = s;
    double L = l;

    if (S == 0.0) {
        r = g = b = L;
        return;
    }

    double q = L < 0.5 ? L * (1.0 + S) : L + S - L * S;
    double p = 2.0 * L - q;

    r = hueToRgb(p, q, H + 1.0 / 3.0);
    g = hueToRgb(p, q, H);
    b = hueToRgb(p, q, H - 1.0 / 3.0);
}

void TriChromaticProcessor::rgbToHsl(double r, double g, double b, double& h, double& s, double& l) {
    double max = std::max({r, g, b});
    double min = std::min({r, g, b});
    l = (max + min) / 2.0;

    if (max == min) {
        h = 0.0;
        s = 0.0;
        return;
    }

    double d = max - min;
    s = l > 0.5 ? d / (2.0 - max - min) : d / (max + min);

    if (max == r)
        h = (g - b) / d + (g < b ? 6.0 : 0.0);
    else if (max == g)
        h = (b - r) / d + 2.0;
    else
        h = (r - g) / d + 4.0;

    h /= 6.0;
    h *= 360.0;
}

// ============================================================
// プリセット
// ============================================================

TriChromaticTriad TriChromaticTriad::cinematic() {
    return {240.0, 0.6, 0.15,  0.0, 0.05, 0.5,  45.0, 0.7, 0.9};
}

TriChromaticTriad TriChromaticTriad::tealAndOrange() {
    return {190.0, 0.5, 0.15,  0.0, 0.05, 0.5,  25.0, 0.6, 0.9};
}

TriChromaticTriad TriChromaticTriad::joker() {
    return {280.0, 0.5, 0.12,  120.0, 0.3, 0.5,  45.0, 0.7, 0.85};
}

TriChromaticTriad TriChromaticTriad::cyberpunk() {
    return {220.0, 0.7, 0.1,   300.0, 0.4, 0.5,  340.0, 0.8, 0.9};
}

// ============================================================
// TriChromaticProcessor
// ============================================================

TriChromaticProcessor::TriChromaticProcessor() = default;
TriChromaticProcessor::~TriChromaticProcessor() = default;

void TriChromaticProcessor::setSettings(const TriChromaticSettings& settings) {
    settings_ = settings;
}

const TriChromaticSettings& TriChromaticProcessor::settings() const {
    return settings_;
}

double TriChromaticProcessor::luma(double r, double g, double b) {
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

double TriChromaticProcessor::smoothstep(double edge0, double edge1, double x) {
    if (edge0 == edge1)
        return x < edge0 ? 0.0 : 1.0;

    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

void TriChromaticProcessor::applyPixel(double& r, double& g, double& b) const {
    const auto& triad = settings_.triad;
    const double hueShift = settings_.hueShift;

    // 現在の輝度
    const double srcLuma = luma(r, g, b);

    // シャドウ/ハイライトのウェイト計算
    const double balance = settings_.balance;
    const double soft = settings_.softness;
    const double shadowW = 1.0 - smoothstep(balance - soft, balance, srcLuma);
    const double highlightW = smoothstep(balance, balance + soft, srcLuma);
    const double midW = std::clamp(1.0 - shadowW - highlightW, 0.0, 1.0);

    // hueShift を適用したベース3色 → RGB
    auto toRgb = [&](double hue, double sat, double lum) {
        double hr, hg, hb;
        hslToRgb(hue + hueShift, sat, lum, hr, hg, hb);
        return std::array<double, 3>{hr, hg, hb};
    };

    auto shadowRgb   = toRgb(triad.shadowHue, triad.shadowSat, triad.shadowLum);
    auto midtoneRgb  = toRgb(triad.midtoneHue, triad.midtoneSat, triad.midtoneLum);
    auto highlightRgb = toRgb(triad.highlightHue, triad.highlightSat, triad.highlightLum);

    // 3色をブレンド
    double tr = shadowRgb[0] * shadowW + midtoneRgb[0] * midW + highlightRgb[0] * highlightW;
    double tg = shadowRgb[1] * shadowW + midtoneRgb[1] * midW + highlightRgb[1] * highlightW;
    double tb = shadowRgb[2] * shadowW + midtoneRgb[2] * midW + highlightRgb[2] * highlightW;

    // strength と colorMix で入力とブレンド
    const double mix = std::clamp(settings_.masterStrength * settings_.colorMix, 0.0, 1.0);
    r = r * (1.0 - mix) + tr * mix;
    g = g * (1.0 - mix) + tg * mix;
    b = b * (1.0 - mix) + tb * mix;

    // 輝度保存
    if (settings_.preserveLuma) {
        const double outLuma = luma(r, g, b);
        if (outLuma > 1e-10) {
            const double scale = srcLuma / outLuma;
            const double preserve = std::clamp(settings_.masterStrength, 0.0, 1.0);
            r = r * (1.0 - preserve) + std::clamp(r * scale, 0.0, 1.0) * preserve;
            g = g * (1.0 - preserve) + std::clamp(g * scale, 0.0, 1.0) * preserve;
            b = b * (1.0 - preserve) + std::clamp(b * scale, 0.0, 1.0) * preserve;
        }
    }

    r = std::clamp(r, 0.0, 1.0);
    g = std::clamp(g, 0.0, 1.0);
    b = std::clamp(b, 0.0, 1.0);
}

void TriChromaticProcessor::applyPixel(float& r, float& g, float& b) const {
    double dr = r, dg = g, db = b;
    applyPixel(dr, dg, db);
    r = static_cast<float>(dr);
    g = static_cast<float>(dg);
    b = static_cast<float>(db);
}

void TriChromaticProcessor::apply(float4* buffer, int width, int height) const {
    if (!buffer || width <= 0 || height <= 0 || settings_.masterStrength <= 0.0)
        return;

    for (int i = 0; i < width * height; ++i) {
        double r = buffer[i].x;
        double g = buffer[i].y;
        double b = buffer[i].z;
        applyPixel(r, g, b);
        buffer[i].x = static_cast<float>(r);
        buffer[i].y = static_cast<float>(g);
        buffer[i].z = static_cast<float>(b);
    }
}

void TriChromaticProcessor::apply(ImageF32x4_RGBA& image) const {
    if (image.isEmpty() || settings_.masterStrength <= 0.0) return;
    float* raw = image.rgba32fData();
    if (!raw) return;
    apply(reinterpret_cast<float4*>(raw), image.width(), image.height());
}

}
