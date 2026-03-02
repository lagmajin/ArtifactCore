module;

#include "../Define/DllExportMacro.hpp"
#include <QList>

export module Color.Harmonizer;

import std;
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
