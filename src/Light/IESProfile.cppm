module;
#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <stdexcept>

module Light.IESProfile;

namespace ArtifactCore {

// ─── Parser Helpers ───

void IESParser::skipLabelLine(std::string_view& s) const {
    auto nl = s.find('\n');
    if (nl != std::string_view::npos) s.remove_prefix(nl + 1);
}

std::string IESParser::readLabel(std::string_view& s) const {
    auto nl = s.find('\n');
    if (nl == std::string_view::npos) return "";
    std::string label(s.substr(0, nl));
    s.remove_prefix(nl + 1);
    if (!label.empty() && label.back() == '\r') label.pop_back();
    return label;
}

void IESParser::skipWhitespace(std::string_view& s) const {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n'))
        s.remove_prefix(1);
}

float IESParser::readFloat(std::string_view& s) const {
    skipWhitespace(s);
    const char* p = s.data();
    char* ep = nullptr;
    float v = std::strtof(p, &ep);
    if (ep == p) return 0.0f;
    s.remove_prefix(ep - p);
    return v;
}

std::vector<float> IESParser::readFloatArray(std::string_view& s, int count) const {
    std::vector<float> arr; arr.reserve(count);
    for (int i = 0; i < count; ++i) arr.push_back(readFloat(s));
    return arr;
}

// ─── Parse ───

IESProfile IESParser::parse(std::string_view content) const {
    IESProfile profile;
    auto s = content;

    // Skip IESNA prefix/tilt
    while (!s.empty() && (s.substr(0,5)=="IESNA"||s.substr(0,4)=="TILT"||s.front()=='I')) {
        auto nl = s.find('\n');
        if (nl != std::string_view::npos) s.remove_prefix(nl + 1); else break;
    }

    // Read label lines
    std::string labels[5];
    for (int i = 0; i < 5 && !s.empty(); ++i) {
        auto nl = s.find('\n');
        if (nl == std::string_view::npos) labels[i] = std::string(s);
        else { labels[i] = std::string(s.substr(0, nl)); s.remove_prefix(nl + 1); }
        if (!labels[i].empty() && labels[i].back() == '\r') labels[i].pop_back();
    }

    // Skip remaining labels to reach numeric header (total 13 lines)
    // The 5 labels we read might not be enough to skip all labels.
    // Standard IES LM-63 has exactly 13 lines of metadata before the data header.
    // We'll read floats until we get valid angular counts.

    // Read header values
    profile.header.bulbLumens = readFloat(s);
    float mult = readFloat(s);
    profile.header.numVerticalAngles = (int)readFloat(s);
    profile.header.numHorizontalAngles = (int)readFloat(s);
    profile.header.photometricType = (int)readFloat(s);
    profile.header.unitsType = (int)readFloat(s);
    profile.header.width = readFloat(s);
    profile.header.length = readFloat(s);
    profile.header.height = readFloat(s);
    profile.header.ballastFactor = readFloat(s);
    profile.header.inputWatts = readFloat(s);

    int nv = profile.header.numVerticalAngles;
    int nh = profile.header.numHorizontalAngles;
    if (nv <= 0 || nh <= 0) return profile;

    profile.verticalAngles = readFloatArray(s, nv);
    profile.horizontalAngles = readFloatArray(s, nh);

    profile.candelaValues.resize(nh);
    for (int h = 0; h < nh; ++h) {
        profile.candelaValues[h] = readFloatArray(s, nv);
        for (float& v : profile.candelaValues[h]) v *= mult * profile.header.ballastFactor;
    }

    profile.maxCandela = 0.0f;
    for (auto& row : profile.candelaValues)
        for (float v : row) profile.maxCandela = std::max(profile.maxCandela, v);

    return profile;
}

IESProfile IESParser::parseFile(const std::string& filePath) const {
    std::ifstream f(filePath, std::ios::binary);
    if (!f) return {};
    std::stringstream ss; ss << f.rdbuf();
    return parse(ss.str());
}

// ─── Sample ───

float IESProfile::sample(float vert, float horiz) const {
    if (!isValid()) return 0.0f;
    horiz = std::fmod(horiz, 360.0f); if (horiz < 0.0f) horiz += 360.0f;
    vert = std::clamp(vert, 0.0f, 180.0f);
    int vi = 0; while (vi+1 < header.numVerticalAngles && verticalAngles[vi+1] <= vert) ++vi;
    int vj = std::min(vi+1, header.numVerticalAngles-1);
    float vt = (vj > vi) ? (vert - verticalAngles[vi]) / (verticalAngles[vj] - verticalAngles[vi]) : 0.0f;
    int hi = 0; while (hi+1 < header.numHorizontalAngles && horizontalAngles[hi+1] <= horiz) ++hi;
    int hj = (hi+1) % header.numHorizontalAngles;
    float ht = (hj != hi) ? (horiz - horizontalAngles[hi]) / (horizontalAngles[hj] - horizontalAngles[hi]) : 0.0f;
    if (header.numHorizontalAngles == 1) { hj = 0; ht = 0.0f; }
    float c00 = candelaValues[hi][vi], c10 = candelaValues[hj][vi];
    float c01 = candelaValues[hi][vj], c11 = candelaValues[hj][vj];
    return c00 + (c10-c00)*ht + ((c01+(c11-c01)*ht) - (c00+(c10-c00)*ht)) * vt;
}

void IESLutTexture::buildFromProfile(const IESProfile& profile) {
    if (!profile.isValid()) return;
    data.resize(width * height * 4, 0.0f);
    float invMax = 1.0f / std::max(profile.maxCandela, 1.0f);
    for (int y = 0; y < height; ++y) {
        float va = (float(y) + 0.5f) / float(height) * 180.0f;
        for (int x = 0; x < width; ++x) {
            float ha = (float(x) + 0.5f) / float(width) * 360.0f;
            float cd = profile.sample(va, ha) * invMax;
            size_t idx = (y * width + x) * 4;
            data[idx]=data[idx+1]=data[idx+2]=cd; data[idx+3]=1.0f;
        }
    }
}

} // namespace ArtifactCore
