module;
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include <array>
#include <tuple>
module StarfieldGenerator;

namespace ArtifactCore {

// ============================================================
// 内部定数: H-R図ベースの恒星統計
// ============================================================

namespace {

// スペクトル型ごとの分布確率 (実際の恒星統計に基づく概算)
const float spectralProbabilities[] = {
    0.000003f, 0.0013f, 0.006f, 0.03f, 0.076f, 0.121f, 0.765f
};

// 各スペクトル型の色温度 (K) - 主系列星 平均値
const float temperatureV[] = {
    40000.0f, 20000.0f, 8500.0f, 6500.0f, 5700.0f, 4500.0f, 3000.0f
};

// 光度階級ごとの絶対等級オフセット (主系列=0基準)
const float luminosityOffset[] = {
    -7.0f, -5.5f, -3.0f, -0.5f, 0.0f, 0.0f, 2.0f, 10.0f
};

// スペクトル型ごとのRGB色味重み (見た目の美しさ重視)
const float spectralRgbWeights[][3] = {
    {0.7f,  0.8f, 1.0f},   // O: 青白
    {0.75f, 0.85f, 1.0f},  // B: 青白
    {0.85f, 0.92f, 1.0f},  // A: 白
    {1.0f,  0.95f, 0.9f},  // F: 黄白
    {1.0f,  0.9f,  0.8f},  // G: 黄
    {1.0f,  0.8f,  0.6f},  // K: 橙
    {0.9f,  0.6f,  0.4f},  // M: 赤
};

// 黒体放射の近似 (温度K → RGB)
void blackbodyToRgb(float tempK, float& r, float& g, float& b) {
    float t = tempK / 1000.0f;
    if (t <= 6.5f) r = 1.0f; else r = 1.0f - 0.3f * (t - 6.5f) / 33.5f;
    r = std::clamp(r, 0.1f, 1.0f);
    if (t <= 5.0f) g = 0.3f + 0.7f * t / 5.0f;
    else if (t <= 12.0f) g = 1.0f;
    else g = 1.0f - 0.5f * (t - 12.0f) / 28.0f;
    g = std::clamp(g, 0.05f, 1.0f);
    if (t <= 4.0f) b = 0.05f + 0.05f * t / 4.0f;
    else if (t <= 8.0f) b = 0.1f + 0.9f * (t - 4.0f) / 4.0f;
    else b = 1.0f;
    b = std::clamp(b, 0.02f, 1.0f);
}

void buildCumulativeProb(const float* probs, int count, float* cumul) {
    float sum = 0.0f;
    for (int i = 0; i < count; ++i) sum += probs[i];
    cumul[0] = probs[0] / sum;
    for (int i = 1; i < count; ++i) cumul[i] = cumul[i - 1] + probs[i] / sum;
}

int sampleFromCumulative(const float* cumul, int count, float rnd) {
    for (int i = 0; i < count; ++i) if (rnd <= cumul[i]) return i;
    return count - 1;
}

float noise2D(float x, float y, unsigned int seed) {
    auto hash = [](int xi, int yi, unsigned int s) -> float {
        unsigned int h = s;
        h = h * 1664525u + 2654435769u;
        h = h ^ (static_cast<unsigned int>(xi) * 0x9E3779B9u);
        h = h * 1664525u + 2654435769u;
        h = h ^ (static_cast<unsigned int>(yi) * 0x9E3779B9u);
        h = h * 1103515245u + 12345u;
        return static_cast<float>(h & 0x7FFFFFFF) / 2147483648.0f;
    };
    int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    float fx = x - xi, fy = y - yi;
    float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    float n00 = hash(xi, yi, seed), n10 = hash(xi + 1, yi, seed);
    float n01 = hash(xi, yi + 1, seed), n11 = hash(xi + 1, yi + 1, seed);
    return (n00 + (n10 - n00) * sx) + ((n01 + (n11 - n01) * sx) - (n00 + (n10 - n00) * sx)) * sy;
}

float fbm(float x, float y, int octaves, unsigned int seed) {
    float value = 0.0f, amplitude = 1.0f, frequency = 1.0f, maxValue = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        value += noise2D(x * frequency, y * frequency, seed + i * 7919u) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5f; frequency *= 2.0f;
    }
    return value / maxValue;
}

} // anonymous namespace

// ============================================================
// StarfieldGenerator 実装
// ============================================================

struct StarfieldGenerator::Impl {
    std::mt19937 rng;
    float spectralCumul[7];
    int starCount = 2000;
    float shootingStarX = 0.0f, shootingStarY = 0.0f;
    float shootingStarLen = 0.0f, shootingStarAngle = 0.0f;
    float shootingStarLife = 0.0f, shootingStarSpeed = 0.0f;
    Impl() : rng(42) {
        buildCumulativeProb(spectralProbabilities, 7, spectralCumul);
    }
};

StarfieldGenerator::StarfieldGenerator() : impl_(std::make_unique<Impl>()) {}
void StarfieldGenerator::setStarCount(int count) { impl_->starCount = std::max(1, count); }
void StarfieldGenerator::setResolution(int width, int height) { width_ = width; height_ = height; }
void StarfieldGenerator::setSeed(unsigned int seed) { impl_->rng.seed(seed); }
void StarfieldGenerator::setMilkyWayIntensity(float v) { milkyWayIntensity_ = std::clamp(v, 0.0f, 1.0f); }
void StarfieldGenerator::setGlareEnabled(bool v) { glareEnabled_ = v; }
void StarfieldGenerator::setGlareThreshold(float v) { glareThreshold_ = std::clamp(v, 0.0f, 1.0f); }
void StarfieldGenerator::setShootingStarChance(float v) { shootingStarChance_ = std::clamp(v, 0.0f, 1.0f); }
void StarfieldGenerator::addNebula(const NebulaData& n) { nebulae_.push_back(n); }
void StarfieldGenerator::clearNebulae() { nebulae_.clear(); }

// ----- スペクトル→色 -----

void StarfieldGenerator::spectralToRgb(SpectralType type, LuminosityClass lc,
                                        float& r, float& g, float& b) {
    int si = std::clamp(static_cast<int>(type), 0, 6);
    float tempK = spectralTemperature(type, lc);
    blackbodyToRgb(tempK, r, g, b);
    r *= spectralRgbWeights[si][0]; g *= spectralRgbWeights[si][1]; b *= spectralRgbWeights[si][2];
}

float StarfieldGenerator::spectralTemperature(SpectralType type, LuminosityClass lc) {
    int si = std::clamp(static_cast<int>(type), 0, 6);
    float base = temperatureV[si];
    int li = std::clamp(static_cast<int>(lc), 0, 7);
    if (li <= 2) base *= 0.9f; else if (li == 3) base *= 0.95f; else if (li >= 6) base *= 1.05f;
    return base;
}

float StarfieldGenerator::absoluteMagnitude(SpectralType type, LuminosityClass lc) {
    int si = std::clamp(static_cast<int>(type), 0, 6);
    int li = std::clamp(static_cast<int>(lc), 0, 7);
    float mvV[] = { -5.0f, -1.5f, 1.5f, 3.0f, 4.8f, 6.5f, 10.0f };
    return mvV[si] + luminosityOffset[li];
}

// ----- 星分布生成 -----

void StarfieldGenerator::generateStarDistribution(unsigned int seed) {
    impl_->rng.seed(seed);
    stars_.clear();
    float cumul[7];
    buildCumulativeProb(spectralProbabilities, 7, cumul);

    for (int i = 0; i < impl_->starCount; ++i) {
        StarData star;
        float rx = std::uniform_real_distribution<float>(0.0f, 1.0f)(impl_->rng);
        float ry = std::uniform_real_distribution<float>(0.0f, 1.0f)(impl_->rng);

        if (milkyWayIntensity_ > 0.0f) {
            float gr = std::uniform_real_distribution<float>(0.0f, 1.0f)(impl_->rng);
            if (gr < milkyWayIntensity_) {
                float bw = 0.2f + 0.1f * milkyWayIntensity_;
                ry = std::clamp(std::normal_distribution<float>(0.5f, bw)(impl_->rng), 0.0f, 1.0f);
            }
        }
        star.x = rx; star.y = ry;

        float sr = std::uniform_real_distribution<float>(0.0f, 1.0f)(impl_->rng);
        SpectralType st = static_cast<SpectralType>(sampleFromCumulative(cumul, 7, sr));

        float lcr = std::uniform_real_distribution<float>(0.0f, 1.0f)(impl_->rng);
        LuminosityClass lc;
        if (lcr < 0.001f) lc = LuminosityClass::Ia;
        else if (lcr < 0.003f) lc = LuminosityClass::Ib;
        else if (lcr < 0.01f) lc = LuminosityClass::II;
        else if (lcr < 0.05f) lc = LuminosityClass::III;
        else if (lcr < 0.08f) lc = LuminosityClass::IV;
        else if (lcr < 0.85f) lc = LuminosityClass::V;
        else if (lcr < 0.95f) lc = LuminosityClass::VI;
        else lc = LuminosityClass::VII;

        spectralToRgb(st, lc, star.r, star.g, star.b);

        float absMag = absoluteMagnitude(st, lc);
        float dist = std::pow(10.0f, std::uniform_real_distribution<float>(1.0f, 5.0f)(impl_->rng));
        float apparentMag = absMag + 5.0f * std::log10(dist / 10.0f);
        star.brightness = std::clamp(1.0f - (apparentMag + 1.0f) / 7.0f, 0.0f, 1.0f);
        star.size = 0.5f + star.brightness * 1.5f;
        star.hasGlare = glareEnabled_ && (star.brightness > glareThreshold_);
        star.twinkleSpeed = std::uniform_real_distribution<float>(0.2f, 3.0f)(impl_->rng);
        star.twinklePhase = std::uniform_real_distribution<float>(0.0f, 6.283185f)(impl_->rng);

        stars_.push_back(star);
    }
}

// ----- レンダリング -----

void StarfieldGenerator::renderStars(float* pixels, float time) {
    for (auto& star : stars_) {
        float twinkle = 1.0f;
        if (star.twinkleSpeed > 0.0f) {
            float p = star.twinklePhase + time * star.twinkleSpeed;
            twinkle = 0.7f + 0.3f * noise2D(star.x * 100.0f + p, star.y * 100.0f, 12345u);
        }
        float fb = star.brightness * twinkle;
        if (fb < 0.01f) continue;

        int cx = static_cast<int>(star.x * (width_ - 1));
        int cy = static_cast<int>(star.y * (height_ - 1));
        float sr = star.size; int iR = std::max(1, static_cast<int>(std::ceil(sr)));

        for (int dy = -iR; dy <= iR; ++dy) for (int dx = -iR; dx <= iR; ++dx) {
            int px = cx + dx, py = cy + dy;
            if (px < 0 || px >= width_ || py < 0 || py >= height_) continue;
            float d = std::sqrt(static_cast<float>(dx*dx+dy*dy));
            float a = 1.0f - std::clamp(d / sr, 0.0f, 1.0f);
            a = a * a * fb;
            int idx = (py*width_+px)*4;
            pixels[idx+0] = std::min(1.0f, pixels[idx+0] + star.r * a);
            pixels[idx+1] = std::min(1.0f, pixels[idx+1] + star.g * a);
            pixels[idx+2] = std::min(1.0f, pixels[idx+2] + star.b * a);
        }
        if (star.hasGlare && fb > glareThreshold_)
            applyGlare(pixels, cx, cy, fb, star.r, star.g, star.b);
    }
}

void StarfieldGenerator::renderNebulae(float* pixels) {
    for (auto& neb : nebulae_) {
        float invW = 1.0f / width_, invH = 1.0f / height_;
        float nrx = 1.0f / neb.radiusX, nry = 1.0f / neb.radiusY;
        for (int y = 0; y < height_; ++y) for (int x = 0; x < width_; ++x) {
            float nx = (x * invW - neb.centerX) * nrx;
            float ny = (y * invH - neb.centerY) * nry;
            float dn = nx * nx + ny * ny;
            float a = std::exp(-dn * 2.0f) * neb.opacity * 0.3f;
            float fb = fbm(nx * 3.0f + 0.5f, ny * 3.0f, 5, 99991u);
            a *= 0.5f + 0.5f * fb;
            if (a > 0.001f) {
                int idx = (y*width_+x)*4;
                pixels[idx+0] = std::min(1.0f, pixels[idx+0] + neb.r * a);
                pixels[idx+1] = std::min(1.0f, pixels[idx+1] + neb.g * a);
                pixels[idx+2] = std::min(1.0f, pixels[idx+2] + neb.b * a);
            }
        }
    }
}

void StarfieldGenerator::renderShootingStars(float* pixels, float time) {
    auto& rng = impl_->rng;
    if (impl_->shootingStarLife > 0.0f) impl_->shootingStarLife -= 0.016f;

    if (impl_->shootingStarLife <= 0.0f && shootingStarChance_ > 0.0f) {
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < shootingStarChance_) {
            impl_->shootingStarX = std::uniform_real_distribution<float>(0.1f, 0.9f)(rng);
            impl_->shootingStarY = std::uniform_real_distribution<float>(0.1f, 0.5f)(rng);
            impl_->shootingStarAngle = std::uniform_real_distribution<float>(-0.5f, 0.5f)(rng);
            impl_->shootingStarLen = std::uniform_real_distribution<float>(0.05f, 0.15f)(rng);
            impl_->shootingStarSpeed = std::uniform_real_distribution<float>(0.3f, 0.8f)(rng);
            impl_->shootingStarLife = 1.0f;
        }
    }

    if (impl_->shootingStarLife > 0.0f) {
        float p = 1.0f - impl_->shootingStarLife;
        float hx = impl_->shootingStarX + std::cos(impl_->shootingStarAngle) * p * impl_->shootingStarSpeed;
        float hy = impl_->shootingStarY + std::sin(impl_->shootingStarAngle) * p * impl_->shootingStarSpeed;
        float tl = impl_->shootingStarLen * impl_->shootingStarLife;
        int steps = static_cast<int>(tl * std::max(width_, height_));
        for (int s = 0; s < steps; ++s) {
            float t = static_cast<float>(s) / steps;
            float sx = hx - std::cos(impl_->shootingStarAngle) * tl * t;
            float sy = hy - std::sin(impl_->shootingStarAngle) * tl * t;
            int px = static_cast<int>(sx * (width_ - 1));
            int py = static_cast<int>(sy * (height_ - 1));
            if (px < 0 || px >= width_ || py < 0 || py >= height_) continue;
            float a = (1.0f - t) * impl_->shootingStarLife * 0.8f;
            int idx = (py*width_+px)*4;
            pixels[idx+0] = std::min(1.0f, pixels[idx+0] + 0.9f * a);
            pixels[idx+1] = std::min(1.0f, pixels[idx+1] + 0.9f * a);
            pixels[idx+2] = std::min(1.0f, pixels[idx+2] + 1.0f * a);
        }
    }
}

// ----- メイン生成 -----

void StarfieldGenerator::generate(float* pixels, float time) {
    if (stars_.empty()) generateStarDistribution(impl_->rng());
    for (int i = 0; i < width_ * height_ * 4; ++i) pixels[i] = backgroundLevel_;

    if (galaxyEnabled_) renderGalaxy(pixels);
    renderNebulae(pixels);
    renderStars(pixels, time);
    renderStarSystems(pixels, time);
    renderGlobularClusters(pixels, time);
    if (constellationLines_) renderConstellationLines(pixels);
    if (shootingStarChance_ > 0.0f) renderShootingStars(pixels, time);
}

// ----- 新規setter -----

void StarfieldGenerator::setGlarePattern(GlarePattern p) { glarePattern_ = p; }
void StarfieldGenerator::setBackgroundLevel(float l) { backgroundLevel_ = std::clamp(l, 0.0f, 1.0f); }
void StarfieldGenerator::setGalaxyEnabled(bool e) { galaxyEnabled_ = e; }
void StarfieldGenerator::setGalaxyCenter(float x, float y) { galaxyCenterX_=x; galaxyCenterY_=y; }
void StarfieldGenerator::setGalaxyRadius(float r) { galaxyRadius_ = std::max(0.01f, r); }
void StarfieldGenerator::setGalaxyArmCount(int c) { galaxyArmCount_ = std::max(1, c); }
void StarfieldGenerator::setGalaxyTilt(float t) { galaxyTilt_ = t; }
void StarfieldGenerator::addStarSystem(const StarSystemData& s) { systems_.push_back(s); }
void StarfieldGenerator::clearStarSystems() { systems_.clear(); }
void StarfieldGenerator::setGlobularClusterCount(int c) { globularClusterCount_ = std::max(0, c); }
void StarfieldGenerator::setConstellationLinesEnabled(bool e) { constellationLines_ = e; }
void StarfieldGenerator::setConstellationLineThreshold(float t) { constellationThreshold_ = t; }
void StarfieldGenerator::setConstellationLineMaxDist(float d) { constellationMaxDist_ = d; }

// ----- 銀河描画 -----

void StarfieldGenerator::renderGalaxy(float* pixels) {
    float cx = galaxyCenterX_ * (width_ - 1);
    float cy = galaxyCenterY_ * (height_ - 1);
    float rMax = galaxyRadius_ * std::max(width_, height_);
    int armCount = galaxyArmCount_;

    for (int y = 0; y < height_; ++y) for (int x = 0; x < width_; ++x) {
        float dx = x - cx, dy = y - cy;
        float rx = dx * std::cos(galaxyTilt_) - dy * std::sin(galaxyTilt_);
        float ry = dx * std::sin(galaxyTilt_) + dy * std::cos(galaxyTilt_);
        float dist = std::sqrt(rx * rx + ry * ry);
        if (dist > rMax * 1.2f) continue;
        float angle = std::atan2(ry, rx);
        float spiral = angle + std::log(dist / (rMax * 0.1f + 1.0f)) * 2.0f;
        float armFactor = 0.0f;
        for (int a = 0; a < armCount; ++a) {
            float phase = a * 6.283185f / armCount;
            armFactor = std::max(armFactor, std::cos(spiral * armCount + phase));
        }
        armFactor = (armFactor + 1.0f) * 0.5f;
        float distFade = 1.0f - std::clamp(dist / rMax, 0.0f, 1.0f);
        distFade *= distFade * 0.4f;
        float armWidth = 0.15f + 0.1f * (1.0f - dist / rMax);
        float armAlpha = std::exp(-(1.0f - armFactor) * (1.0f - armFactor) / (armWidth * armWidth)) * distFade;
        float bulge = std::exp(-dist * dist / (rMax * rMax * 0.02f)) * 0.6f;
        float totalAlpha = std::max(armAlpha, bulge);
        if (totalAlpha > 0.001f) {
            int idx = (y * width_ + x) * 4;
            float br = 1.0f - dist / rMax;
            pixels[idx+0] = std::min(1.0f, pixels[idx+0] + (0.85f + 0.15f * br) * totalAlpha);
            pixels[idx+1] = std::min(1.0f, pixels[idx+1] + (0.75f + 0.25f * br) * totalAlpha);
            pixels[idx+2] = std::min(1.0f, pixels[idx+2] + (0.5f + 0.5f * br) * totalAlpha);
        }
    }
}

void StarfieldGenerator::drawDisc(float* pixels, float cx, float cy, float radius,
                                   float r, float g, float b, float alpha) {
    int iR = std::max(1, static_cast<int>(std::ceil(radius * std::max(width_, height_))));
    int icx = static_cast<int>(cx), icy = static_cast<int>(cy);
    float pxR = radius * std::max(width_, height_);
    for (int dy = -iR; dy <= iR; ++dy) for (int dx = -iR; dx <= iR; ++dx) {
        int px = icx + dx, py = icy + dy;
        if (px < 0 || px >= width_ || py < 0 || py >= height_) continue;
        float d = std::sqrt(static_cast<float>(dx*dx+dy*dy));
        float edge = 1.0f - std::clamp(d / pxR, 0.0f, 1.0f);
        float a = (edge > 0.5f ? 1.0f : edge * 2.0f) * alpha;
        int idx = (py*width_+px)*4;
        pixels[idx+0] = std::min(1.0f, pixels[idx+0] + r * a);
        pixels[idx+1] = std::min(1.0f, pixels[idx+1] + g * a);
        pixels[idx+2] = std::min(1.0f, pixels[idx+2] + b * a);
    }
}

void StarfieldGenerator::drawRing(float* pixels, float cx, float cy,
                                    float innerR, float outerR,
                                    float r, float g, float b, float alpha,
                                    float startAngle, float endAngle) {
    float screenScale = std::max(width_, height_);
    float inR = innerR * screenScale, outR = outerR * screenScale;
    int iR = std::max(1, static_cast<int>(std::ceil(outR)));
    int icx = static_cast<int>(cx), icy = static_cast<int>(cy);
    for (int dy = -iR; dy <= iR; ++dy) for (int dx = -iR; dx <= iR; ++dx) {
        int px = icx + dx, py = icy + dy;
        if (px < 0 || px >= width_ || py < 0 || py >= height_) continue;
        float d = std::sqrt(static_cast<float>(dx*dx+dy*dy));
        if (d < inR || d > outR) continue;
        float ang = std::atan2(static_cast<float>(dy), static_cast<float>(dx));
        if (ang < 0.0f) ang += 6.283185f;
        if (ang < startAngle || ang > endAngle) continue;
        float edge = std::min((d - inR) / (outR - inR + 1.0f), (outR - d) / (outR - inR + 1.0f));
        int idx = (py*width_+px)*4;
        pixels[idx+0] = std::min(1.0f, pixels[idx+0] + r * edge * alpha);
        pixels[idx+1] = std::min(1.0f, pixels[idx+1] + g * edge * alpha);
        pixels[idx+2] = std::min(1.0f, pixels[idx+2] + b * edge * alpha);
    }
}

void StarfieldGenerator::renderStarSystems(float* pixels, float time) {
    float screenScale = std::max(width_, height_);
    for (auto& sys : systems_) {
        float scx = sys.centerX * (width_ - 1), scy = sys.centerY * (height_ - 1);
        float br = sys.star.brightness;
        drawDisc(pixels, scx, scy, sys.star.size * 0.002f, sys.star.r, sys.star.g, sys.star.b, br);
        if (sys.star.hasGlare)
            applyGlare(pixels, static_cast<int>(scx), static_cast<int>(scy), br, sys.star.r, sys.star.g, sys.star.b);
        for (auto& p : sys.planets) {
            float angle = p.orbitPhase + time * p.orbitSpeed;
            float rOrbit = p.orbitRadius * screenScale;
            float px = scx + std::cos(angle) * rOrbit;
            float py = scy + std::sin(angle) * rOrbit * (1.0f - p.eccentricity * 0.5f);
            float pr = p.planetRadius * screenScale;
            if (p.hasAtmosphere)
                drawDisc(pixels, px, py, pr * 1.4f, p.atmosphereR, p.atmosphereG, p.atmosphereB, 0.15f);
            drawDisc(pixels, px, py, pr, p.r, p.g, p.b, 0.9f);
            if (p.hasRing)
                drawRing(pixels, px, py, pr * p.ringInner, pr * p.ringOuter, p.ringR, p.ringG, p.ringB, 0.4f);
            if (sys.showOrbits) {
                int steps = std::max(8, static_cast<int>(rOrbit * 0.5f));
                float px0 = 0, py0 = 0;
                for (int s = 0; s <= steps; ++s) {
                    float a = s * 6.283185f / steps;
                    float ox = scx + std::cos(a) * rOrbit;
                    float oy = scy + std::sin(a) * rOrbit * (1.0f - p.eccentricity * 0.5f);
                    if (s > 0) {
                        int fx=static_cast<int>(px0),fy=static_cast<int>(py0),tx=static_cast<int>(ox),ty=static_cast<int>(oy);
                        int ss2 = std::max(std::abs(tx-fx),std::abs(ty-fy));
                        for (int k=0;k<=ss2;++k){float t=ss2>0?static_cast<float>(k)/ss2:0.0f;
                            int lx=fx+static_cast<int>((tx-fx)*t),ly=fy+static_cast<int>((ty-fy)*t);
                            if(lx>=0&&lx<width_&&ly>=0&&ly<height_){int idx=(ly*width_+lx)*4;
                            float la=sys.orbitLineAlpha;
                            pixels[idx+0]=std::min(1.0f,pixels[idx+0]+0.15f*la);
                            pixels[idx+1]=std::min(1.0f,pixels[idx+1]+0.15f*la);
                            pixels[idx+2]=std::min(1.0f,pixels[idx+2]+0.20f*la);}}
                    }
                    px0=ox;py0=oy;
                }
            }
        }
    }
}


// ----- 球状星団 + 星座線 + グレア更新 -----

void StarfieldGenerator::renderGlobularClusters(float* pixels, float time) {
    auto& rng = impl_->rng;
    for (int c = 0; c < globularClusterCount_; ++c) {
        float gcCx = std::uniform_real_distribution<float>(0.2f, 0.8f)(rng) * (width_-1);
        float gcCy = std::uniform_real_distribution<float>(0.2f, 0.8f)(rng) * (height_-1);
        float gcR = std::uniform_real_distribution<float>(0.03f, 0.08f)(rng) * std::max(width_, height_);
        int n = std::uniform_int_distribution<int>(50, 200)(rng);
        for (int s = 0; s < n; ++s) {
            float d = std::abs(std::normal_distribution<float>(0.0f, gcR*0.3f)(rng));
            if (d > gcR) continue;
            float a = std::uniform_real_distribution<float>(0.0f, 6.283185f)(rng);
            int sx = static_cast<int>(gcCx + std::cos(a)*d);
            int sy = static_cast<int>(gcCy + std::sin(a)*d);
            if (sx<0||sx>=width_||sy<0||sy>=height_) continue;
            float cr,cg,cb,tr=std::uniform_real_distribution<float>(0.0f,1.0f)(rng);
            if(tr<0.7f){cr=1.0f;cg=0.8f;cb=0.5f;}else if(tr<0.9f){cr=1.0f;cg=0.9f;cb=0.7f;}else{cr=0.7f;cg=0.8f;cb=1.0f;}
            float br2 = std::uniform_real_distribution<float>(0.3f, 0.8f)(rng);
            int idx = (sy*width_+sx)*4;
            pixels[idx+0]=std::min(1.0f,pixels[idx+0]+cr*br2);
            pixels[idx+1]=std::min(1.0f,pixels[idx+1]+cg*br2);
            pixels[idx+2]=std::min(1.0f,pixels[idx+2]+cb*br2);
        }
    }
}

void StarfieldGenerator::renderConstellationLines(float* pixels) {
    float maxDist = constellationMaxDist_ * std::max(width_, height_);
    for (size_t i = 0; i < stars_.size(); ++i) {
        if (stars_[i].brightness < constellationThreshold_) continue;
        for (size_t j = i + 1; j < stars_.size(); ++j) {
            if (stars_[j].brightness < constellationThreshold_) continue;
            float dx=(stars_[i].x-stars_[j].x)*width_, dy=(stars_[i].y-stars_[j].y)*height_;
            if (std::sqrt(dx*dx+dy*dy)>maxDist) continue;
            int fx=static_cast<int>(stars_[i].x*(width_-1)),fy=static_cast<int>(stars_[i].y*(height_-1));
            int tx=static_cast<int>(stars_[j].x*(width_-1)),ty=static_cast<int>(stars_[j].y*(height_-1));
            int steps=std::max(std::abs(tx-fx),std::abs(ty-fy));
            for(int s=0;s<=steps;++s){float t=steps>0?static_cast<float>(s)/steps:0.0f;
                int lx=fx+static_cast<int>((tx-fx)*t),ly=fy+static_cast<int>((ty-fy)*t);
                if(lx>=0&&lx<width_&&ly>=0&&ly<height_){int idx=(ly*width_+lx)*4;
                pixels[idx+0]=std::min(1.0f,pixels[idx+0]+0.08f);
                pixels[idx+1]=std::min(1.0f,pixels[idx+1]+0.08f);
                pixels[idx+2]=std::min(1.0f,pixels[idx+2]+0.12f);}}
        }
    }
}

void StarfieldGenerator::applyGlare(float* pixels, int cx, int cy, float brightness, float r, float g, float b) {
    if (!glareEnabled_ || brightness <= glareThreshold_) return;
    float baseLen=10.0f+brightness*20.0f, baseW=1.0f+brightness*1.5f;
    int numSpikes=4; float spikeBoost=1.0f;
    switch(glarePattern_){case GlarePattern::Cross4:numSpikes=4;break;case GlarePattern::Cross6:numSpikes=6;break;
    case GlarePattern::Cross8:numSpikes=8;break;case GlarePattern::Starburst:numSpikes=8;spikeBoost=0.6f;break;default:break;}
    for(int s=0;s<numSpikes;++s){float angle=s*6.283185f/numSpikes;
        if(glarePattern_==GlarePattern::Starburst)angle+=std::uniform_real_distribution<float>(-0.3f,0.3f)(impl_->rng);
        float sl=baseLen*spikeBoost;int ilen=static_cast<int>(sl);float ca=std::cos(angle),sa=std::sin(angle);
        for(int t=-ilen;t<=ilen;++t){int px=cx+static_cast<int>(t*ca),py=cy+static_cast<int>(t*sa);
            if(px<0||px>=width_||py<0||py>=height_)continue;
            float d=std::abs(static_cast<float>(t)),a=std::exp(-d*d/(sl*sl*0.3f))*brightness*0.25f;
            for(int w=-static_cast<int>(baseW);w<=static_cast<int>(baseW);++w){
                int wx=px+static_cast<int>(-sa*w),wy=py+static_cast<int>(ca*w);
                if(wx<0||wx>=width_||wy<0||wy>=height_)continue;
                float wa=a*std::max(0.0f,1.0f-std::abs(static_cast<float>(w))/baseW);
                int idx=(wy*width_+wx)*4;
                pixels[idx+0]=std::min(1.0f,pixels[idx+0]+r*wa);
                pixels[idx+1]=std::min(1.0f,pixels[idx+1]+g*wa);
                pixels[idx+2]=std::min(1.0f,pixels[idx+2]+b*wa);}}}
    float hr=3.0f+brightness*8.0f;int hR=static_cast<int>(hr);
    for(int dy=-hR;dy<=hR;++dy)for(int dx=-hR;dx<=hR;++dx){
        int px=cx+dx,py=cy+dy;if(px<0||px>=width_||py<0||py>=height_)continue;
        float d=std::sqrt(static_cast<float>(dx*dx+dy*dy)),a=std::exp(-d*d/(hr*hr*0.15f))*brightness*0.15f;
        int idx=(py*width_+px)*4;pixels[idx+0]=std::min(1.0f,pixels[idx+0]+r*a);
        pixels[idx+1]=std::min(1.0f,pixels[idx+1]+g*a);pixels[idx+2]=std::min(1.0f,pixels[idx+2]+b*a);}
}

} // namespace ArtifactCore

