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

export namespace ArtifactCore {

/**
 * @brief 自動カラーマッチング
 * 
 * 2つの画像間でカラーを一致させる。
 * 映画ポスプロで頻用される Reinhard Color Transfer と
 * Mean/Stddev ベースの手法を実装。
 */
class LIBRARY_DLL_API AutoColorMatcher {
public:
    /// カラーマッチングの手法
    enum class Method {
        Reinhard,       // Reinhard et al. (2001) — Lab空間で統計量を合わせる
        MeanStddev,     // RGB空間でmean/stddevを合わせる (軽量版)
        Histogram       // ヒストグラムマッチング
    };

    /// カラーマッチングの結果パラメータ
    struct MatchResult {
        // RGB乗数とオフセット (簡易適用用)
        float scaleR = 1.0f, scaleG = 1.0f, scaleB = 1.0f;
        float offsetR = 0.0f, offsetG = 0.0f, offsetB = 0.0f;
        float confidence = 0.0f;  // 0-1, マッチの信頼度
    };

    /// ソース画像をリファレンス画像のカラーに合わせる
    /// @param srcPixels   入力/出力バッファ (インプレース変更)
    /// @param refPixels   リファレンス画像バッファ (読み取りのみ)
    /// @param width, height  両画像は同じサイズを想定
    /// @param method      マッチング手法
    /// @param intensity   適用強度 (0=変更なし, 1=完全マッチ)
    static void match(float* srcPixels, const float* refPixels,
                      int width, int height,
                      Method method = Method::Reinhard,
                      float intensity = 1.0f);

    /// マッチングパラメータだけを計算 (適用はしない)
    static MatchResult computeMatch(const float* srcPixels, const float* refPixels,
                                     int srcWidth, int srcHeight,
                                     int refWidth, int refHeight,
                                     Method method = Method::Reinhard);

    /// 計算済みパラメータを画像に適用
    static void applyMatch(float* pixels, int width, int height,
                            const MatchResult& result, float intensity = 1.0f);

    /// Lab空間での Reinhard Color Transfer
    /// 画像サイズが異なっても可能
    static void reinhardTransfer(float* srcPixels, int srcWidth, int srcHeight,
                                  const float* refPixels, int refWidth, int refHeight,
                                  float intensity = 1.0f);

    /// RGB空間でのMean/Stddev マッチング (軽量)
    static void meanStddevMatch(float* srcPixels, int srcWidth, int srcHeight,
                                 const float* refPixels, int refWidth, int refHeight,
                                 float intensity = 1.0f);

    /// ヒストグラムマッチング
    static void histogramMatch(float* srcPixels, int srcWidth, int srcHeight,
                                const float* refPixels, int refWidth, int refHeight,
                                float intensity = 1.0f);
};

} // namespace ArtifactCore
