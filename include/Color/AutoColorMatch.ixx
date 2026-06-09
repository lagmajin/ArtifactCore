module;

#include "../Define/DllExportMacro.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Color.AutoMatch;

import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

class LIBRARY_DLL_API AutoColorMatcher {
public:
    enum class Method {
        Reinhard,
        MeanStddev,
        Histogram
    };

    struct MatchResult {
        float scaleR = 1.0f, scaleG = 1.0f, scaleB = 1.0f;
        float offsetR = 0.0f, offsetG = 0.0f, offsetB = 0.0f;
        float confidence = 0.0f;
    };

    static void match(float* srcPixels, const float* refPixels,
                      int width, int height,
                      Method method = Method::Reinhard,
                      float intensity = 1.0f);

    static MatchResult computeMatch(const float* srcPixels, const float* refPixels,
                                     int srcWidth, int srcHeight,
                                     int refWidth, int refHeight,
                                     Method method = Method::Reinhard);

    static void applyMatch(float* pixels, int width, int height,
                            const MatchResult& result, float intensity = 1.0f);

    static void reinhardTransfer(float* srcPixels, int srcWidth, int srcHeight,
                                  const float* refPixels, int refWidth, int refHeight,
                                  float intensity = 1.0f);

    static void meanStddevMatch(float* srcPixels, int srcWidth, int srcHeight,
                                 const float* refPixels, int refWidth, int refHeight,
                                 float intensity = 1.0f);

    static void histogramMatch(float* srcPixels, int srcWidth, int srcHeight,
                                const float* refPixels, int refWidth, int refHeight,
                                float intensity = 1.0f);

    static void match(ImageF32x4_RGBA& src, const ImageF32x4_RGBA& ref,
                      Method method = Method::Reinhard,
                      float intensity = 1.0f);

    static void reinhardTransfer(ImageF32x4_RGBA& src, const ImageF32x4_RGBA& ref,
                                  float intensity = 1.0f);

    static void meanStddevMatch(ImageF32x4_RGBA& src, const ImageF32x4_RGBA& ref,
                                 float intensity = 1.0f);

    static void histogramMatch(ImageF32x4_RGBA& src, const ImageF32x4_RGBA& ref,
                                float intensity = 1.0f);
};

}
