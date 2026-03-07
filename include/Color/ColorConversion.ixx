module;

#include "../Define/DllExportMacro.hpp"

export module Color.Conversion;

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>




export namespace ArtifactCore {

struct HSVColor {
    float h; // 0.0 - 360.0 degrees
    float s; // 0.0 - 1.0
    float v; // 0.0 - 1.0
};

struct HSLColor {
    float h; // 0.0 - 360.0
    float s; // 0.0 - 1.0
    float l; // 0.0 - 1.0
};

class LIBRARY_DLL_API ColorConversion {
public:
    static HSVColor RGBToHSV(float r, float g, float b);
    static std::array<float, 3> HSVToRGB(const HSVColor& hsv);

    static HSLColor RGBToHSL(float r, float g, float b);
    static std::array<float, 3> HSLToRGB(const HSLColor& hsl);
};

} // namespace ArtifactCore
