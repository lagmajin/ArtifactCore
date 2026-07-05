module;
#include <algorithm>
#include <array>
#include <cmath>
#include <QImage>

export module ImageProcessing.ColorTransform.SelectiveColor;

export namespace ArtifactCore {

enum class SelectiveColorGroup {
    Reds = 0,
    Yellows = 1,
    Greens = 2,
    Cyans = 3,
    Blues = 4,
    Magentas = 5,
    Whites = 6,
    Neutrals = 7,
    Blacks = 8,
    Count = 9,
};

struct SelectiveColorAdjustment {
    float cyan = 0.0f;
    float magenta = 0.0f;
    float yellow = 0.0f;
    float black = 0.0f;
};

struct SelectiveColorSettings {
    std::array<SelectiveColorAdjustment, static_cast<size_t>(SelectiveColorGroup::Count)> groups{};
    float strength = 1.0f;
    bool relativeMode = true;
    bool preserveLuma = false;

    void reset() {
        *this = SelectiveColorSettings{};
    }

    static SelectiveColorSettings neutral();
    static SelectiveColorSettings warm();
    static SelectiveColorSettings cool();
    static SelectiveColorSettings vivid();
    static SelectiveColorSettings film();
};

class SelectiveColorProcessor {
public:
    SelectiveColorProcessor();
    ~SelectiveColorProcessor();

    void setSettings(const SelectiveColorSettings& settings);
    const SelectiveColorSettings& settings() const;

    QImage apply(const QImage& source) const;
    void applyPixel(float& r, float& g, float& b) const;

private:
    SelectiveColorSettings settings_;

    struct RangeWeights {
        float reds = 0.0f;
        float yellows = 0.0f;
        float greens = 0.0f;
        float cyans = 0.0f;
        float blues = 0.0f;
        float magentas = 0.0f;
        float whites = 0.0f;
        float neutrals = 0.0f;
        float blacks = 0.0f;
    };

    static float luma(float r, float g, float b);
    static float smoothstep(float edge0, float edge1, float x);
    static float hueDistance(float a, float b);
    static RangeWeights computeWeights(float r, float g, float b);
    static void preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled, float strength);
};

} // namespace ArtifactCore
