module;

#include "../Define/DllExportMacro.hpp"

export module ColorCollection.ColorGrading;

import std;
import Color.ColorSpace;

// Color wheel types for different correction methods
export namespace ArtifactCore {

// Color wheel type
enum class ColorWheelType {
    RGB,               // Combined RGB wheel
    LiftGammaGain,     // Lift/Gamma/Gain wheels
    OffsetGammaGain,   // DaVinci-style Offset/Gamma/Gain
    ShadowsMidtonesHighlights // Three-way corrector
};

// Single color wheel parameters (Lift/Gamma/Gain)
struct ColorWheelParams {
    // Lift (shadows) - adds to blacks
    float liftR = 0.0f;
    float liftG = 0.0f;
    float liftB = 0.0f;
    float liftMaster = 0.0f;
    
    // Gamma (midtones) - gamma correction
    float gammaR = 1.0f;
    float gammaG = 1.0f;
    float gammaB = 1.0f;
    float gammaMaster = 1.0f;
    
    // Gain (highlights) - multiplies
    float gainR = 1.0f;
    float gainG = 1.0f;
    float gainB = 1.0f;
    float gainMaster = 1.0f;
    
    // Offset - additive adjustment (for Offset/Gamma/Gain)
    float offsetR = 0.0f;
    float offsetG = 0.0f;
    float offsetB = 0.0f;
    float offsetMaster = 0.0f;
    
    // Reset to defaults
    void reset() {
        liftR = liftG = liftB = liftMaster = 0;
        gammaR = gammaG = gammaB = gammaMaster = 1;
        gainR = gainG = gainB = gainMaster = 1;
        offsetR = offsetG = offsetB = offsetMaster = 0;
    }
    
    // Check if all values are default
    bool isDefault() const {
        return liftR == 0 && liftG == 0 && liftB == 0 && liftMaster == 0 &&
               gammaR == 1 && gammaG == 1 && gammaB == 1 && gammaMaster == 1 &&
               gainR == 1 && gainG == 1 && gainB == 1 && gainMaster == 1 &&
               offsetR == 0 && offsetG == 0 && offsetB == 0 && offsetMaster == 0;
    }
};

// Three-way color corrector (Shadows/Midtones/Highlights)
struct ThreeWayColorParams {
    ColorWheelParams shadows;    // Lift
    ColorWheelParams midtones;   // Gamma  
    ColorWheelParams highlights; // Gain
    
    // Master controls
    float masterSaturation = 1.0f;
    float masterContrast = 1.0f;
    float masterBrightness = 0.0f;
    
    void reset() {
        shadows.reset();
        midtones.reset();
        highlights.reset();
        masterSaturation = 1.0f;
        masterContrast = 1.0f;
        masterBrightness = 0.0f;
    }
};

// Color levels adjustment
struct ColorLevelsParams {
    // Input range
    float inputBlack = 0.0f;
    float inputWhite = 1.0f;
    
    // Output range
    float outputBlack = 0.0f;
    float outputWhite = 1.0f;
    
    // Gamma correction
    float gamma = 1.0f;
    
    // Per-channel controls
    bool linkRGB = true;
    float inputBlackR = 0.0f, inputWhiteR = 1.0f;
    float inputBlackG = 0.0f, inputWhiteG = 1.0f;
    float inputBlackB = 0.0f, inputWhiteB = 1.0f;
    
    void reset() {
        inputBlack = inputBlackR = inputBlackG = inputBlackB = 0;
        inputWhite = inputWhiteR = inputWhiteG = inputWhiteB = 1;
        outputBlack = 0;
        outputWhite = 1;
        gamma = 1;
        linkRGB = true;
    }
    
    bool isDefault() const {
        return inputBlack == 0 && inputWhite == 1 &&
               outputBlack == 0 && outputWhite == 1 &&
               gamma == 1 && linkRGB;
    }
};

// Color wheel processor - processes images with lift/gamma/gain
class LIBRARY_DLL_API ColorWheelsProcessor {
public:
    ColorWheelsProcessor();
    ~ColorWheelsProcessor();
    
    // Type
    void setWheelType(ColorWheelType type);
    ColorWheelType wheelType() const { return type_; }
    
    // Lift/Gamma/Gain
    ColorWheelParams& wheels() { return wheels_; }
    const ColorWheelParams& wheels() const { return wheels_; }
    
    void setLift(float r, float g, float b);
    void setGamma(float r, float g, float b);
    void setGain(float r, float g, float b);
    void setOffset(float r, float g, float b);
    void setLiftMaster(float v);
    void setGammaMaster(float v);
    void setGainMaster(float v);
    void setOffsetMaster(float v);
    
    // Three-way corrector
    ThreeWayColorParams& threeWay() { return threeWay_; }
    const ThreeWayColorParams& threeWay() const { return threeWay_; }
    
    // Levels
    ColorLevelsParams& levels() { return levels_; }
    const ColorLevelsParams& levels() const { return levels_; }
    
    // Processing - processes RGBA float pixels in place
    void process(float* pixels, int width, int height);
    
    // Single pixel processing
    void processPixel(float& r, float& g, float& b);
    
    // Presets
    static ColorWheelsProcessor createWarmLook();
    static ColorWheelsProcessor createCoolLook();
    static ColorWheelsProcessor createHighContrast();
    static ColorWheelsProcessor createFade();
    
    // Reset all
    void reset();
    
private:
    void applyLift(float& r, float& g, float& b);
    void applyGamma(float& r, float& g, float& b);
    void applyGain(float& r, float& g, float& b);
    void applyOffset(float& r, float& g, float& b);
    void applyLevels(float& r, float& g, float& b);
    void applyThreeWay(float& r, float& g, float& b);
    float calculateLuminance(float r, float g, float b) const;
    
    ColorWheelType type_ = ColorWheelType::LiftGammaGain;
    ColorWheelParams wheels_;
    ThreeWayColorParams threeWay_;
    ColorLevelsParams levels_;
};

// Curve control point
struct CurvePoint {
    float x;  // 0-1 range, input
    float y;  // 0-1 range, output
    float handleInX = 0.0f;
    float handleInY = 0.0f;
    float handleOutX = 0.0f;
    float handleOutY = 0.0f;
};

// Color curves - RGB curve adjustment
class LIBRARY_DLL_API ColorCurves {
public:
    ColorCurves();
    ~ColorCurves();
    
    // Master curve
    void setMasterCurve(const std::vector<CurvePoint>& points);
    const std::vector<CurvePoint>& masterCurve() const { return masterCurve_; }
    void addMasterPoint(float x, float y);
    void removeMasterPoint(size_t index);
    void clearMasterPoints();
    
    // Channel curves
    void setRedCurve(const std::vector<CurvePoint>& points);
    void setGreenCurve(const std::vector<CurvePoint>& points);
    void setBlueCurve(const std::vector<CurvePoint>& points);
    
    const std::vector<CurvePoint>& redCurve() const { return redCurve_; }
    const std::vector<CurvePoint>& greenCurve() const { return greenCurve_; }
    const std::vector<CurvePoint>& blueCurve() const { return blueCurve_; }
    
    // Evaluate curve at position
    float evaluateMaster(float x) const;
    float evaluateRed(float x) const;
    float evaluateGreen(float x) const;
    float evaluateBlue(float x) const;
    
    // Predefined curves
    void applySCurve();
    void applyFadeIn();
    void applyFadeOut();
    void applyInvert();
    void applyPosterize(int levels);
    
    // Build lookup table from curves
    void buildLUT();
    
    // Processing
    void process(float* pixels, int width, int height);
    void processPixel(float& r, float& g, float& b);
    
    // Reset
    void reset();
    bool isDefault() const;
    
private:
    float interpolateCurve(const std::vector<CurvePoint>& curve, float x) const;
    
    std::vector<CurvePoint> masterCurve_;
    std::vector<CurvePoint> redCurve_;
    std::vector<CurvePoint> greenCurve_;
    std::vector<CurvePoint> blueCurve_;
    
    std::array<float, 256> masterLUT_;
    std::array<float, 256> redLUT_;
    std::array<float, 256> greenLUT_;
    std::array<float, 256> blueLUT_;
    
    bool lutValid_ = false;
};

// Color grader - combines wheels, curves, levels
class LIBRARY_DLL_API ColorGrader {
public:
    ColorGrader();
    ~ColorGrader();
    
    ColorWheelsProcessor& wheels() { return wheelsProcessor_; }
    const ColorWheelsProcessor& wheels() const { return wheelsProcessor_; }
    
    ColorCurves& curves() { return curvesProcessor_; }
    const ColorCurves& curves() const { return curvesProcessor_; }
    
    ColorLevelsParams& levels() { return levels_; }
    const ColorLevelsParams& levels() const { return levels_; }
    
    void setEnabled(bool e) { enabled_ = e; }
    bool isEnabled() const { return enabled_; }
    
    void setIntensity(float i) { intensity_ = std::clamp(i, 0.0f, 1.0f); }
    float intensity() const { return intensity_; }
    
    void process(float* pixels, int width, int height);
    void processPixel(float& r, float& g, float& b);
    
    // Presets
    static ColorGrader createFilmLook();
    static ColorGrader createCinematic();
    static ColorGrader createNoir();
    
    void reset();
    
private:
    ColorWheelsProcessor wheelsProcessor_;
    ColorCurves curvesProcessor_;
    ColorLevelsParams levels_;
    
    bool enabled_ = true;
    float intensity_ = 1.0f;
};

} // namespace ArtifactCore
