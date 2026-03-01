module;

#include "../Define/DllExportMacro.hpp"
#include <vector>

export module Color.Harmonizer;

import std;
import Color.Conversion;

export namespace ArtifactCore {

class LIBRARY_DLL_API ColorHarmonizer {
public:
    // 補色 (ComplementaryColor) : 180度反対の色
    static std::array<float, 3> getComplementary(float r, float g, float b);

    // 類似色 (AnalogousColors) : 指定した角度（通常30度）隣の色
    static std::vector<std::array<float, 3>> getAnalogous(float r, float g, float b, float angle = 30.0f);

    // 三補色 (TriadicColors) : 120度ずつの3色
    static std::vector<std::array<float, 3>> getTriadic(float r, float g, float b);

    // 分裂補色 (Split-Complementary) : 180度の両隣（例：150度と210度）
    static std::vector<std::array<float, 3>> getSplitComplementary(float r, float g, float b, float offsetAngle = 30.0f);

    // 四補色 (Tetradic / Square) : 90度ずつの4色
    static std::vector<std::array<float, 3>> getTetradic(float r, float g, float b);
};

} // namespace ArtifactCore
