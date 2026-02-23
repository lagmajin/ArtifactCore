module;

export module ColorCollection.ColorGrading;

import std;
import Color.ColorSpace;

namespace ArtifactCore {

// ColorWheelsProcessor implementation
ColorWheelsProcessor::ColorWheelsProcessor() = default;
ColorWheelsProcessor::~ColorWheelsProcessor() = default;

void ColorWheelsProcessor::setWheelType(ColorWheelType type) {
    type_ = type;
}

void ColorWheelsProcessor::setLift(float r, float g, float b) {
    wheels_.liftR = r;
    wheels_.liftG = g;
    wheels_.liftB = b;
}

void ColorWheelsProcessor::setGamma(float r, float g, float b) {
    wheels_.gammaR = r;
    wheels_.gammaG = g;
    wheels_.gammaB = b;
}

void ColorWheelsProcessor::setGain(float r, float g, float b) {
    wheels_.gainR = r;
    wheels_.gainG = g;
    wheels_.gainB = b;
}

void ColorWheelsProcessor::setOffset(float r, float g, float b) {
    wheels_.offsetR = r;
    wheels_.offsetG = g;
    wheels_.offsetB = b;
}

void ColorWheelsProcessor::setLiftMaster(float v) { wheels_.liftMaster = v; }
void ColorWheelsProcessor::setGammaMaster(float v) { wheels_.gammaMaster = v; }
void ColorWheelsProcessor::setGainMaster(float v) { wheels_.gainMaster = v; }
void ColorWheelsProcessor::setOffsetMaster(float v) { wheels_.offsetMaster = v; }

float ColorWheelsProcessor::calculateLuminance(float r, float g, float b) const {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

void ColorWheelsProcessor::applyLift(float& r, float& g, float& b) {
    float lum = calculateLuminance(r, g, b);
    float oneMinusLum = 1.0f - lum;
    float masterEffect = wheels_.liftMaster;
    r += (wheels_.liftR + masterEffect) * oneMinusLum;
    g += (wheels_.liftG + masterEffect) * oneMinusLum;
    b += (wheels_.liftB + masterEffect) * oneMinusLum;
}

void ColorWheelsProcessor::applyGamma(float& r, float& g, float& b) {
    float gammaR = wheels_.gammaR * wheels_.gammaMaster;
    float gammaG = wheels_.gammaG * wheels_.gammaMaster;
    float gammaB = wheels_.gammaB * wheels_.gammaMaster;
    
    if (gammaR != 1.0f && gammaR > 0.0f)
        r = std::pow(std::clamp(r, 0.0f, 1.0f), 1.0f / gammaR);
    if (gammaG != 1.0f && gammaG > 0.0f)
        g = std::pow(std::clamp(g, 0.0f, 1.0f), 1.0f / gammaG);
    if (gammaB != 1.0f && gammaB > 0.0f)
        b = std::pow(std::clamp(b, 0.0f, 1.0f), 1.0f / gammaB);
}

void ColorWheelsProcessor::applyGain(float& r, float& g, float& b) {
    float lum = calculateLuminance(r, g, b);
    float masterEffect = wheels_.gainMaster;
    r *= (wheels_.gainR + masterEffect * lum);
    g *= (wheels_.gainG + masterEffect * lum);
    b *= (wheels_.gainB + masterEffect * lum);
}

void ColorWheelsProcessor::applyOffset(float& r, float& g, float& b) {
    float masterEffect = wheels_.offsetMaster;
    r += wheels_.offsetR + masterEffect;
    g += wheels_.offsetG + masterEffect;
    b += wheels_.offsetB + masterEffect;
}

void ColorWheelsProcessor::applyLevels(float& r, float& g, float& b) {
    const auto& lvl = levels_;
    
    if (lvl.linkRGB) {
        if (r < lvl.inputBlack) r = lvl.inputBlack;
        else if (r > lvl.inputWhite) r = lvl.inputWhite;
        r = (r - lvl.inputBlack) / (lvl.inputWhite - lvl.inputBlack);
        g = r; b = r;
        
        if (lvl.gamma != 1.0f) {
            r = std::pow(std::clamp(r, 0.0f, 1.0f), 1.0f / lvl.gamma);
            g = r; b = r;
        }
        
        r = lvl.outputBlack + r * (lvl.outputWhite - lvl.outputBlack);
        g = r; b = r;
    }
}

void ColorWheelsProcessor::applyThreeWay(float& r, float& g, float& b) {
    float lum = calculateLuminance(r, g, b);
    float shadowWeight = 1.0f - std::clamp(lum * 2.0f, 0.0f, 1.0f);
    shadowWeight = shadowWeight * shadowWeight;
    float highlightWeight = std::clamp((lum - 0.5f) * 2.0f, 0.0f, 1.0f);
    highlightWeight = highlightWeight * highlightWeight;
    float midtoneWeight = 1.0f - shadowWeight - highlightWeight;
    
    r += threeWay_.shadows.liftR * shadowWeight;
    g += threeWay_.shadows.liftG * shadowWeight;
    b += threeWay_.shadows.liftB * shadowWeight;
    
    if (threeWay_.midtones.gammaR != 1.0f) r = std::pow(std::clamp(r, 0.0f, 1.0f), 1.0f / threeWay_.midtones.gammaR);
    if (threeWay_.midtones.gammaG != 1.0f) g = std::pow(std::clamp(g, 0.0f, 1.0f), 1.0f / threeWay_.midtones.gammaG);
    if (threeWay_.midtones.gammaB != 1.0f) b = std::pow(std::clamp(b, 0.0f, 1.0f), 1.0f / threeWay_.midtones.gammaB);
    
    r *= (1.0f + threeWay_.highlights.gainR * highlightWeight);
    g *= (1.0f + threeWay_.highlights.gainG * highlightWeight);
    b *= (1.0f + threeWay_.highlights.gainB * highlightWeight);
    
    if (threeWay_.masterSaturation != 1.0f) {
        float gray = lum;
        r = gray + (r - gray) * threeWay_.masterSaturation;
        g = gray + (g - gray) * threeWay_.masterSaturation;
        b = gray + (b - gray) * threeWay_.masterSaturation;
    }
    
    if (threeWay_.masterContrast != 1.0f) {
        r = (r - 0.5f) * threeWay_.masterContrast + 0.5f;
        g = (g - 0.5f) * threeWay_.masterContrast + 0.5f;
        b = (b - 0.5f) * threeWay_.masterContrast + 0.5f;
    }
    
    r += threeWay_.masterBrightness;
    g += threeWay_.masterBrightness;
    b += threeWay_.masterBrightness;
}

void ColorWheelsProcessor::processPixel(float& r, float& g, float& b) {
    if (type_ == ColorWheelType::ShadowsMidtonesHighlights) {
        applyThreeWay(r, g, b);
    } else {
        applyLift(r, g, b);
        applyGamma(r, g, b);
        applyGain(r, g, b);
        if (type_ == ColorWheelType::OffsetGammaGain) applyOffset(r, g, b);
    }
    if (!levels_.isDefault()) applyLevels(r, g, b);
    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
}

void ColorWheelsProcessor::process(float* pixels, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        float* pixel = pixels + i * 4;
        processPixel(pixel[0], pixel[1], pixel[2]);
    }
}

ColorWheelsProcessor ColorWheelsProcessor::createWarmLook() {
    ColorWheelsProcessor proc;
    proc.setGain(1.1f, 1.0f, 0.9f);
    proc.setLift(0.05f, 0.02f, 0.0f);
    return proc;
}

ColorWheelsProcessor ColorWheelsProcessor::createCoolLook() {
    ColorWheelsProcessor proc;
    proc.setGain(0.9f, 1.0f, 1.1f);
    proc.setLift(0.0f, 0.02f, 0.05f);
    return proc;
}

ColorWheelsProcessor ColorWheelsProcessor::createHighContrast() {
    ColorWheelsProcessor proc;
    proc.setGain(1.3f, 1.3f, 1.3f);
    proc.setGamma(0.8f, 0.8f, 0.8f);
    return proc;
}

ColorWheelsProcessor ColorWheelsProcessor::createFade() {
    ColorWheelsProcessor proc;
    proc.setGain(0.8f, 0.8f, 0.8f);
    proc.setGamma(1.2f, 1.2f, 1.2f);
    return proc;
}

void ColorWheelsProcessor::reset() {
    wheels_.reset();
    threeWay_.reset();
    levels_.reset();
}

// ColorCurves implementation
ColorCurves::ColorCurves() {
    masterCurve_ = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    redCurve_ = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    greenCurve_ = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    blueCurve_ = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    for (int i = 0; i < 256; i++) {
        masterLUT_[i] = redLUT_[i] = greenLUT_[i] = blueLUT_[i] = i / 255.0f;
    }
    lutValid_ = true;
}

ColorCurves::~ColorCurves() = default;

float ColorCurves::interpolateCurve(const std::vector<CurvePoint>& curve, float x) const {
    if (curve.empty()) return x;
    if (curve.size() == 1) return curve[0].y;
    
    size_t idx = 0;
    for (size_t i = 0; i < curve.size() - 1; i++) {
        if (x >= curve[i].x && x <= curve[i + 1].x) { idx = i; break; }
    }
    const auto& p0 = curve[idx];
    const auto& p1 = curve[idx + 1];
    float t = (x - p0.x) / (p1.x - p0.x);
    t = std::clamp(t, 0.0f, 1.0f);
    return p0.y + t * (p1.y - p0.y);
}

void ColorCurves::setMasterCurve(const std::vector<CurvePoint>& points) { masterCurve_ = points; lutValid_ = false; }
void ColorCurves::addMasterPoint(float x, float y) { masterCurve_.push_back({x, y}); std::sort(masterCurve_.begin(), masterCurve_.end(), [](const CurvePoint& a, const CurvePoint& b){ return a.x < b.x; }); lutValid_ = false; }
void ColorCurves::removeMasterPoint(size_t index) { if (index < masterCurve_.size() && masterCurve_.size() > 2) { masterCurve_.erase(masterCurve_.begin() + index); lutValid_ = false; } }
void ColorCurves::clearMasterPoints() { masterCurve_ = {{0.0f, 0.0f}, {1.0f, 1.0f}}; lutValid_ = false; }
void ColorCurves::setRedCurve(const std::vector<CurvePoint>& points) { redCurve_ = points; lutValid_ = false; }
void ColorCurves::setGreenCurve(const std::vector<CurvePoint>& points) { greenCurve_ = points; lutValid_ = false; }
void ColorCurves::setBlueCurve(const std::vector<CurvePoint>& points) { blueCurve_ = points; lutValid_ = false; }

float ColorCurves::evaluateMaster(float x) const { return interpolateCurve(masterCurve_, std::clamp(x, 0.0f, 1.0f)); }
float ColorCurves::evaluateRed(float x) const { return interpolateCurve(redCurve_, std::clamp(x, 0.0f, 1.0f)); }
float ColorCurves::evaluateGreen(float x) const { return interpolateCurve(greenCurve_, std::clamp(x, 0.0f, 1.0f)); }
float ColorCurves::evaluateBlue(float x) const { return interpolateCurve(blueCurve_, std::clamp(x, 0.0f, 1.0f)); }

void ColorCurves::applySCurve() { masterCurve_ = {{0.0f, 0.0f}, {0.25f, 0.1f}, {0.5f, 0.5f}, {0.75f, 0.9f}, {1.0f, 1.0f}}; lutValid_ = false; }
void ColorCurves::applyFadeIn() { masterCurve_ = {{0.0f, 0.0f}, {0.5f, 0.2f}, {1.0f, 1.0f}}; lutValid_ = false; }
void ColorCurves::applyFadeOut() { masterCurve_ = {{0.0f, 0.0f}, {0.5f, 0.8f}, {1.0f, 1.0f}}; lutValid_ = false; }
void ColorCurves::applyInvert() { masterCurve_ = {{0.0f, 1.0f}, {1.0f, 0.0f}}; lutValid_ = false; }
void ColorCurves::applyPosterize(int levels) { masterCurve_.clear(); float step = 1.0f / (levels - 1); for (int i = 0; i < levels; i++) { float y = std::round(i * step * (levels - 1)) / (levels - 1); masterCurve_.push_back({i * step, y}); } lutValid_ = false; }

void ColorCurves::buildLUT() {
    for (int i = 0; i < 256; i++) {
        float x = i / 255.0f;
        masterLUT_[i] = evaluateMaster(x);
        redLUT_[i] = evaluateRed(x);
        greenLUT_[i] = evaluateGreen(x);
        blueLUT_[i] = evaluateBlue(x);
    }
    lutValid_ = true;
}

void ColorCurves::processPixel(float& r, float& g, float& b) {
    if (!lutValid_) buildLUT();
    int idx = static_cast<int>(std::clamp(r, 0.0f, 1.0f) * 255);
    r = masterLUT_[idx];
    idx = static_cast<int>(std::clamp(g, 0.0f, 1.0f) * 255);
    g = masterLUT_[idx];
    idx = static_cast<int>(std::clamp(b, 0.0f, 1.0f) * 255);
    b = masterLUT_[idx];
    
    float origR = r, origG = g, origB = b;
    idx = static_cast<int>(std::clamp(origR, 0.0f, 1.0f) * 255); r = redLUT_[idx];
    idx = static_cast<int>(std::clamp(origG, 0.0f, 1.0f) * 255); g = greenLUT_[idx];
    idx = static_cast<int>(std::clamp(origB, 0.0f, 1.0f) * 255); b = blueLUT_[idx];
}

void ColorCurves::process(float* pixels, int width, int height) {
    if (!lutValid_) buildLUT();
    for (int i = 0; i < width * height; i++) {
        float* pixel = pixels + i * 4;
        processPixel(pixel[0], pixel[1], pixel[2]);
    }
}

void ColorCurves::reset() {
    masterCurve_ = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    redCurve_ = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    greenCurve_ = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    blueCurve_ = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    for (int i = 0; i < 256; i++) masterLUT_[i] = redLUT_[i] = greenLUT_[i] = blueLUT_[i] = i / 255.0f;
    lutValid_ = true;
}

bool ColorCurves::isDefault() const {
    bool masterDefault = masterCurve_.size() == 2 && masterCurve_[0].x == 0 && masterCurve_[0].y == 0 && masterCurve_[1].x == 1 && masterCurve_[1].y == 1;
    bool redDefault = redCurve_.size() == 2 && redCurve_[0].x == 0 && redCurve_[0].y == 0 && redCurve_[1].x == 1 && redCurve_[1].y == 1;
    bool greenDefault = greenCurve_.size() == 2 && greenCurve_[0].x == 0 && greenCurve_[0].y == 0 && greenCurve_[1].x == 1 && greenCurve_[1].y == 1;
    bool blueDefault = blueCurve_.size() == 2 && blueCurve_[0].x == 0 && blueCurve_[0].y == 0 && blueCurve_[1].x == 1 && blueCurve_[1].y == 1;
    return masterDefault && redDefault && greenDefault && blueDefault;
}

// ColorGrader implementation
ColorGrader::ColorGrader() = default;
ColorGrader::~ColorGrader() = default;

void ColorGrader::processPixel(float& r, float& g, float& b) {
    if (!enabled_) return;
    float origR = r, origG = g, origB = b;
    wheelsProcessor_.processPixel(r, g, b);
    curvesProcessor_.processPixel(r, g, b);
    if (!levels_.isDefault()) {
        wheelsProcessor_.levels() = levels_;
        wheelsProcessor_.processPixel(r, g, b);
    }
    if (intensity_ < 1.0f) {
        r = origR + (r - origR) * intensity_;
        g = origG + (g - origG) * intensity_;
        b = origB + (b - origB) * intensity_;
    }
}

void ColorGrader::process(float* pixels, int width, int height) {
    if (!enabled_) return;
    for (int i = 0; i < width * height; i++) {
        float* pixel = pixels + i * 4;
        processPixel(pixel[0], pixel[1], pixel[2]);
    }
}

ColorGrader ColorGrader::createFilmLook() {
    ColorGrader grader;
    grader.wheels().setGain(1.1f, 1.0f, 0.95f);
    grader.wheels().setGamma(0.95f, 1.0f, 1.05f);
    grader.curves().applyFadeIn();
    return grader;
}

ColorGrader ColorGrader::createCinematic() {
    ColorGrader grader;
    grader.wheels().setGain(1.15f, 1.0f, 0.85f);
    grader.wheels().setLift(0.0f, 0.02f, 0.04f);
    grader.wheels().setGamma(0.9f, 0.95f, 1.0f);
    grader.curves().applySCurve();
    return grader;
}

ColorGrader ColorGrader::createNoir() {
    ColorGrader grader;
    grader.wheels().threeWay().masterSaturation = 0.0f;
    grader.wheels().threeWay().masterContrast = 1.3f;
    grader.curves().applyFadeOut();
    return grader;
}

void ColorGrader::reset() {
    wheelsProcessor_.reset();
    curvesProcessor_.reset();
    levels_.reset();
    enabled_ = true;
    intensity_ = 1.0f;
}

} // namespace ArtifactCore
