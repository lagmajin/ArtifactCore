module;
#include <QImage>
#include <vector>
#include <algorithm>
#include <cmath>

export module Color.Grading.ColorCurves;

export namespace ArtifactCore {

// ============================================================
// Curve Point
// ============================================================

struct CurvePoint {
    float input = 0.0f;     // 0-1
    float output = 0.0f;    // 0-1
    float inTangent = 0.0f; // bezier in tangent
    float outTangent = 0.0f; // bezier out tangent
    bool smooth = false;     // continuous tangent
};

// ============================================================
// Color Curve (Single Channel)
// ============================================================

class ColorCurve {
public:
    ColorCurve() {
        // Default: linear (0,0) to (1,1)
        points_.push_back({0.0f, 0.0f});
        points_.push_back({1.0f, 1.0f});
    }

    void setPoints(const std::vector<CurvePoint>& pts) { points_ = pts; sortPoints(); }
    const std::vector<CurvePoint>& points() const { return points_; }

    void addPoint(float input, float output) {
        points_.push_back({input, output});
        sortPoints();
    }

    void removePoint(int index) {
        if (index >= 0 && index < static_cast<int>(points_.size()) && points_.size() > 2) {
            points_.erase(points_.begin() + index);
        }
    }

    // Evaluate curve at input t (0-1)
    float evaluate(float t) const {
        if (points_.empty()) return t;
        if (points_.size() == 1) return points_[0].output;

        t = std::clamp(t, 0.0f, 1.0f);

        // Find segment
        int seg = 0;
        for (int i = 0; i < static_cast<int>(points_.size()) - 1; ++i) {
            if (t >= points_[i].input && t <= points_[i + 1].input) {
                seg = i;
                break;
            }
            if (i == static_cast<int>(points_.size()) - 2) {
                seg = i;
            }
        }

        const auto& p0 = points_[seg];
        const auto& p1 = points_[seg + 1];

        float segLen = p1.input - p0.input;
        if (segLen < 1e-6f) return p0.output;

        float localT = (t - p0.input) / segLen;

        if (p0.smooth || p1.smooth) {
            // Cubic bezier interpolation
            float cp0Out = p0.output + p0.outTangent * segLen * 0.333f;
            float cp1In = p1.output - p1.inTangent * segLen * 0.333f;
            float u = 1.0f - localT;
            return u * u * u * p0.output + 3 * u * u * localT * cp0Out +
                   3 * u * localT * localT * cp1In + localT * localT * localT * p1.output;
        }

        // Linear
        return p0.output + (p1.output - p0.output) * localT;
    }

    // Generate LUT (256 entries)
    std::array<uint8_t, 256> generateLUT() const {
        std::array<uint8_t, 256> lut;
        for (int i = 0; i < 256; ++i) {
            float t = static_cast<float>(i) / 255.0f;
            float v = std::clamp(evaluate(t), 0.0f, 1.0f);
            lut[i] = static_cast<uint8_t>(std::round(v * 255.0f));
        }
        return lut;
    }

    // Reset to linear
    void reset() {
        points_.clear();
        points_.push_back({0.0f, 0.0f});
        points_.push_back({1.0f, 1.0f});
    }

private:
    std::vector<CurvePoint> points_;

    void sortPoints() {
        std::sort(points_.begin(), points_.end(),
            [](const CurvePoint& a, const CurvePoint& b) { return a.input < b.input; });
    }
};

// ============================================================
// RGB Color Curves
// ============================================================

class ColorCurves {
public:
    ColorCurve& master() { return master_; }
    const ColorCurve& master() const { return master_; }

    ColorCurve& red() { return red_; }
    const ColorCurve& red() const { return red_; }

    ColorCurve& green() { return green_; }
    const ColorCurve& green() const { return green_; }

    ColorCurve& blue() { return blue_; }
    const ColorCurve& blue() const { return blue_; }

    bool isMasterOnly() const { return masterOnly_; }
    void setMasterOnly(bool v) { masterOnly_ = v; }

    // Apply curves to an image
    QImage apply(const QImage& source) const {
        QImage result = source.convertToFormat(QImage::Format_RGBA8888);

        auto masterLUT = master_.generateLUT();
        auto redLUT = red_.generateLUT();
        auto greenLUT = green_.generateLUT();
        auto blueLUT = blue_.generateLUT();

        const int w = result.width();
        const int h = result.height();

        for (int y = 0; y < h; ++y) {
            uint8_t* row = result.scanLine(y);
            for (int x = 0; x < w; ++x) {
                int idx = x * 4;
                if (masterOnly_) {
                    row[idx] = masterLUT[row[idx]];
                    row[idx + 1] = masterLUT[row[idx + 1]];
                    row[idx + 2] = masterLUT[row[idx + 2]];
                } else {
                    row[idx] = redLUT[masterLUT[row[idx]]];
                    row[idx + 1] = greenLUT[masterLUT[row[idx + 1]]];
                    row[idx + 2] = blueLUT[masterLUT[row[idx + 2]]];
                }
            }
        }

        return result;
    }

    // Reset all curves
    void reset() {
        master_.reset();
        red_.reset();
        green_.reset();
        blue_.reset();
    }

private:
    ColorCurve master_;
    ColorCurve red_;
    ColorCurve green_;
    ColorCurve blue_;
    bool masterOnly_ = true;
};

}
