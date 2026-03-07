module;

#include "../Define/DllExportMacro.hpp"
#include <QList>

export module Color.Harmonizer;

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



import Color.Float;

export namespace ArtifactCore {

class LIBRARY_DLL_API ColorHarmonizer {
public:
    // 補色 (ComplementaryColor) : 180度反対の色
    static FloatColor getComplementary(const FloatColor& color);

    // 類似色 (AnalogousColors) : 指定した角度（通常30度）隣の色
    static QList<FloatColor> getAnalogous(const FloatColor& color, float angle = 30.0f);

    // 三補色 (TriadicColors) : 120度ずつの3色
    static QList<FloatColor> getTriadic(const FloatColor& color);

    // 分裂補色 (Split-Complementary) : 180度の両隣（例：150度と210度）
    static QList<FloatColor> getSplitComplementary(const FloatColor& color, float offsetAngle = 30.0f);

    // 四補色 (Tetradic / Square) : 90度ずつの4色
    static QList<FloatColor> getTetradic(const FloatColor& color);

    // モノクロマティック (Monochromatic) : 同じ色相で明度・彩度を変えた色のリスト
    static QList<FloatColor> getMonochromatic(const FloatColor& color, int count = 3);
};

} // namespace ArtifactCore
