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
export module GenerateTestImage;

export namespace ArtifactCore {

/**
 * @brief テスト用画像生成ユーティリティ
 * 
 * カラーバー、グラデーション、チェッカーボード等のテスト画像を
 * RGBA float バッファに生成する。
 */
class LIBRARY_DLL_API TestImageGenerator {
public:
    /// SMPTE カラーバー (HD用 75%レベル)
    static void colorBars(float* pixels, int width, int height);

    /// SMPTE カラーバー (100%レベル)
    static void colorBars100(float* pixels, int width, int height);

    /// 水平グラデーション (黒→白)
    static void horizontalGradient(float* pixels, int width, int height);

    /// 垂直グラデーション (黒→白)
    static void verticalGradient(float* pixels, int width, int height);

    /// RGBグラデーション (左:R, 中:G, 右:B → 上が明るい)
    static void rgbGradient(float* pixels, int width, int height);

    /// チェッカーボードパターン
    /// @param cellSize  セルのピクセルサイズ (デフォルト 32)
    static void checkerboard(float* pixels, int width, int height, int cellSize = 32);

    /// グレーランプ (11段階ゾーンシステム)
    static void zoneSystem(float* pixels, int width, int height);

    /// 単色塗りつぶし
    static void solidColor(float* pixels, int width, int height,
                           float r, float g, float b, float a = 1.0f);

    /// 円形グラデーション (中心から放射状)
    static void radialGradient(float* pixels, int width, int height);

    /// カラーホイール (中心が白、外周がHue回転)
    static void colorWheel(float* pixels, int width, int height);
};

} // namespace ArtifactCore
