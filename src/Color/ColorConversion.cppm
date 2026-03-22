module;

module Color.Conversion;

import std;

namespace ArtifactCore {

HSVColor ColorConversion::RGBToHSV(float r, float g, float b) {
    float minRGB = std::min({r, g, b});
    float maxRGB = std::max({r, g, b});
    float delta = maxRGB - minRGB;

    HSVColor hsv;
    hsv.v = maxRGB;

    if (delta < 0.00001f) {
        hsv.s = 0;
        hsv.h = 0; // Undefined, but 0 is safe
        return hsv;
    }

    if (maxRGB > 0.0f) {
        hsv.s = (delta / maxRGB);
    } else {
        hsv.s = 0;
        hsv.h = 0;
        return hsv;
    }

    if (r >= maxRGB)
        hsv.h = (g - b) / delta;
    else if (g >= maxRGB)
        hsv.h = 2.0f + (b - r) / delta;
    else
        hsv.h = 4.0f + (r - g) / delta;

    hsv.h *= 60.0f;
    if (hsv.h < 0.0f) hsv.h += 360.0f;

    return hsv;
}

std::array<float, 3> ColorConversion::HSVToRGB(const HSVColor& hsv) {
    float h = hsv.h;
    float s = hsv.s;
    float v = hsv.v;

    if (s <= 0.0f) return {v, v, v};

    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    h /= 60.0f;

    int hh = static_cast<int>(h);
    float ff = h - hh;
    float p = v * (1.0f - s);
    float q = v * (1.0f - (s * ff));
    float t = v * (1.0f - (s * (1.0f - ff)));

    switch (hh) {
    case 0: return {v, t, p};
    case 1: return {q, v, p};
    case 2: return {p, v, t};
    case 3: return {p, q, v};
    case 4: return {t, p, v};
    case 5:
    default: return {v, p, q};
    }
}

HSLColor ColorConversion::RGBToHSL(float r, float g, float b) {
    float maxRGB = std::max({r, g, b});
    float minRGB = std::min({r, g, b});
    float h, s, l = (maxRGB + minRGB) / 2.0f;

    if (maxRGB == minRGB) {
        h = s = 0; // Achromatic
    } else {
        float d = maxRGB - minRGB;
        s = l > 0.5f ? d / (2.0f - maxRGB - minRGB) : d / (maxRGB + minRGB);
        if (maxRGB == r)
            h = (g - b) / d + (g < b ? 6.0f : 0.0f);
        else if (maxRGB == g)
            h = (b - r) / d + 2.0f;
        else
            h = (r - g) / d + 4.0f;
        h /= 6.0f;
    }
    return {h * 360.0f, s, l};
}

std::array<float, 3> ColorConversion::HSLToRGB(const HSLColor& hsl) {
    float h = hsl.h / 360.0f;
    float s = hsl.s;
    float l = hsl.l;

    if (s == 0) return {l, l, l};

    auto hue2rgb = [](float p, float q, float t) {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f / 2.0f) return q;
        if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        return p;
    };

    float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;
    return {hue2rgb(p, q, h + 1.0f / 3.0f), 
            hue2rgb(p, q, h), 
            hue2rgb(p, q, h - 1.0f / 3.0f)};
}

} // namespace ArtifactCore
