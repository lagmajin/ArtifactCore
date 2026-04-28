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
export module Color.BlendMode;




import Color.Float;

export namespace ArtifactCore {

// 一般的に利用されるブレンドモードの列挙
enum class BlendMode {
    Normal,
    Add,            // 加算 (Linear Dodge)
    Subtract,       // 減算
    Multiply,       // 乗算
    Screen,         // スクリーン
    Overlay,        // オーバーレイ
    Darken,         // 比較（暗）
    Lighten,        // 比較（明）
    ColorDodge,     // 覆い焼きカラー
    ColorBurn,      // 焼き込みカラー
    HardLight,      // ハードライト
    SoftLight,      // ソフトライト
    Difference,     // 差像
    Exclusion,      // 除外
    Hue,            // 色相
    Saturation,     // 彩度
    Color,          // カラー
    Luminosity,     // 輝度
    LinearBurn,     // リニアバーン
    Divide,         // 除算
    PinLight,       // ピンライト
    VividLight,     // ビビッドライト
    LinearLight,    // リニアライト
    HardMix         // ハードミックス
};

// ブレンド処理ユーティリティ (CPU向け)
class LIBRARY_DLL_API ColorBlendMode {
public:
    // base (背面) と blend (前面) を指定された opacity (0.0~1.0) と mode で合成
    static FloatColor blend(const FloatColor& base, const FloatColor& blendColor, BlendMode mode, float opacity = 1.0f);

private:
    // 各ブレンドアルゴリズムの静的ヘルパー (RGB別)
    static float blendAdd(float b, float f);
    static float blendSubtract(float b, float f);
    static float blendMultiply(float b, float f);
    static float blendScreen(float b, float f);
    static float blendOverlay(float b, float f);
    static float blendDarken(float b, float f);
    static float blendLighten(float b, float f);
    static float blendColorDodge(float b, float f);
    static float blendColorBurn(float b, float f);
    static float blendHardLight(float b, float f);
    static float blendSoftLight(float b, float f);
    static float blendDifference(float b, float f);
    static float blendExclusion(float b, float f);
    static float blendLinearBurn(float b, float f);
    static float blendDivide(float b, float f);
    static float blendPinLight(float b, float f);
    static float blendVividLight(float b, float f);
    static float blendLinearLight(float b, float f);
    static float blendHardMix(float b, float f);
};

} // namespace ArtifactCore
