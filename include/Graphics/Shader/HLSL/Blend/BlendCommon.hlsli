#ifndef BLEND_COMMON_HLSLI
#define BLEND_COMMON_HLSLI

float3 RgbToHsl(float3 rgb)
{
    float maxC = max(max(rgb.r, rgb.g), rgb.b);
    float minC = min(min(rgb.r, rgb.g), rgb.b);
    float delta = maxC - minC;

    float h = 0.0;
    float s = 0.0;
    float l = (maxC + minC) * 0.5;

    if (delta > 1e-5)
    {
        s = (l < 0.5) ? (delta / (maxC + minC)) : (delta / (2.0 - maxC - minC));

        if (maxC == rgb.r)
            h = fmod(((rgb.g - rgb.b) / delta) + (rgb.g < rgb.b ? 6.0 : 0.0), 6.0);
        else if (maxC == rgb.g)
            h = ((rgb.b - rgb.r) / delta) + 2.0;
        else
            h = ((rgb.r - rgb.g) / delta) + 4.0;

        h /= 6.0;
    }

    return float3(h, s, l);
}

float HueToRgb(float p, float q, float t)
{
    t = fmod(t, 1.0);
    if (t < 0.0) t += 1.0;
    if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
    if (t < 0.5) return q;
    if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
}

float3 HslToRgb(float3 hsl)
{
    float h = hsl.x;
    float s = hsl.y;
    float l = hsl.z;

    if (s < 1e-5)
        return float3(l, l, l);

    float q = (l < 0.5) ? (l * (1.0 + s)) : (l + s - l * s);
    float p = 2.0 * l - q;

    return float3(
        HueToRgb(p, q, h + 1.0 / 3.0),
        HueToRgb(p, q, h),
        HueToRgb(p, q, h - 1.0 / 3.0)
    );
}

float3 SetLuminance(float3 color, float targetL)
{
    float currentL = dot(color, float3(0.299, 0.587, 0.114));
    if (abs(currentL) < 1e-5)
        return float3(0.0, 0.0, 0.0);
    float scale = targetL / currentL;
    return saturate(color * scale);
}

float3 SetSaturation(float3 color, float targetS)
{
    float minC = min(min(color.r, color.g), color.b);
    float maxC = max(max(color.r, color.g), color.b);
    if (maxC - minC < 1e-5)
        return float3(0.0, 0.0, 0.0);

    float3 sorted = color;
    if (sorted.r > sorted.g) { float t = sorted.r; sorted.r = sorted.g; sorted.g = t; }
    if (sorted.g > sorted.b) { float t = sorted.g; sorted.g = sorted.b; sorted.b = t; }
    if (sorted.r > sorted.g) { float t = sorted.r; sorted.r = sorted.g; sorted.g = t; }

    float minOut = sorted.r;
    float midOut = sorted.g;
    float maxOut = sorted.b;

    if (maxOut > minOut)
    {
        midOut = ((midOut - minOut) * targetS) / (maxOut - minOut);
        maxOut = targetS;
    }
    else
    {
        midOut = 0.0;
        maxOut = 0.0;
    }
    minOut = 0.0;

    float3 result;
    if (color.r <= color.g && color.r <= color.b)
        result.r = minOut;
    else if (color.g <= color.r && color.g <= color.b)
        result.g = minOut;
    else
        result.b = minOut;

    if (color.r >= color.g && color.r >= color.b)
        result.r = maxOut;
    else if (color.g >= color.r && color.g >= color.b)
        result.g = maxOut;
    else
        result.b = maxOut;

    bool rMid = (color.r != min(result.r, min(result.g, result.b))) &&
                (color.r != max(result.r, max(result.g, result.b)));
    bool gMid = (color.g != min(result.r, min(result.g, result.b))) &&
                (color.g != max(result.r, max(result.g, result.b)));

    if (rMid) result.r = midOut;
    else if (gMid) result.g = midOut;
    else result.b = midOut;

    return result;
}

float Luminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

#endif
