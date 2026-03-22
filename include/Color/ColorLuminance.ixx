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
export module Color.Luminance;





export namespace ArtifactCore {

enum class LuminanceStandard {
    Rec601,    // SDTV (0.299R + 0.587G + 0.114B)
    Rec709,    // HDTV (0.2126R + 0.7152G + 0.0722B) - Physically correct for sRGB
    Rec2020,   // UHDTV (0.2627R + 0.6780G + 0.0593B)
};

class LIBRARY_DLL_API ColorLuminance {
public:
    // 単純な加重平均による輝度計算 (0.0 - 1.0)
    static float calculate(float r, float g, float b, LuminanceStandard standard = LuminanceStandard::Rec709);

    // 知覚的な明るさ (Perceptual Brightness / Lightness)
    // 0.299R + 0.587G + 0.114B ではなく、より人間に近い感覚
    static float calculatePerceptual(float r, float g, float b);

    // 特定の標準に基づいたグレースケール値を返す (R=G=B=Y)
    static std::array<float, 3> toGrayscale(float r, float g, float b, LuminanceStandard standard = LuminanceStandard::Rec709);
};

} // namespace ArtifactCore
