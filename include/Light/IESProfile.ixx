module;
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

export module Light.IESProfile;

export namespace ArtifactCore {

// ─── IES LM-63 Photometric Data ───

struct IESHeader {
    std::string manufacturer;
    std::string luminaireCatalogNumber;
    std::string luminaireDescription;
    std::string lampCatalogNumber;
    std::string lampDescription;

    int    numVerticalAngles = 0;
    int    numHorizontalAngles = 0;
    int    photometricType = 1;    // 1=C, 2=B, 3=A
    int    unitsType = 1;          // 1=feet, 2=meters
    float  width = 0.0f;
    float  length = 0.0f;
    float  height = 0.0f;
    float  ballastFactor = 1.0f;
    float  bulbLumens = 0.0f;
    float  inputWatts = 0.0f;
};

struct IESProfile {
    IESHeader header;
    std::vector<float> verticalAngles;    // 0..180 degrees
    std::vector<float> horizontalAngles;  // 0..360 degrees (0 only for symmetric)
    std::vector<std::vector<float>> candelaValues; // [horiz][vert]
    float maxCandela = 0.0f;

    bool isValid() const {
        return !candelaValues.empty() && !verticalAngles.empty() &&
               header.numVerticalAngles > 0 && header.numHorizontalAngles > 0;
    }

    // Sample candela at given angles (degrees). Bilinear interpolation.
    float sample(float verticalDeg, float horizontalDeg) const;
};

// ─── IES File Parser ───

class IESParser {
public:
    IESProfile parse(std::string_view rawFileContent) const;
    IESProfile parseFile(const std::string& filePath) const;

private:
    void skipLabelLine(std::string_view& s) const;
    std::string readLabel(std::string_view& s) const;
    void skipWhitespace(std::string_view& s) const;
    float readFloat(std::string_view& s) const;
    std::vector<float> readFloatArray(std::string_view& s, int count) const;
};

// ─── 2D LUT Texture Builder ───

struct IESLutTexture {
    int width = 256;   // horizontal samples
    int height = 128;  // vertical samples
    std::vector<float> data; // RGBA16F: 4 floats per pixel (intensity packed in R)

    void buildFromProfile(const IESProfile& profile);
    const float* rawData() const { return data.data(); }
    size_t dataSize() const { return data.size() * sizeof(float); }
};

} // namespace ArtifactCore
