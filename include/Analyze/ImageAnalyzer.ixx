module;

#include "../Define/DllExportMacro.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

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
namespace ArtifactCore {
struct ImageSurfaceView;
struct ImageByteSurfaceView;
}
export module Analyze.Histogram;

export namespace ArtifactCore {

/// 1チャンネル分のヒストグラムと統計情報
struct ChannelStatistics {
    float histogram[256]{};     // 正規化済みヒストグラム (各ビンの頻度 0-1)
    int   rawHistogram[256]{};  // 生のピクセルカウント
    float min = 1.0f;           // 最小値
    float max = 0.0f;           // 最大値
    float mean = 0.0f;          // 平均値
    float median = 0.0f;        // 中央値
    float stddev = 0.0f;        // 標準偏差
    float percentile5 = 0.0f;   // 5パーセンタイル
    float percentile95 = 0.0f;  // 95パーセンタイル
    int   totalPixels = 0;
};

/// RGBA 4チャンネル分の統計情報
struct ImageStatistics {
    ChannelStatistics red;
    ChannelStatistics green;
    ChannelStatistics blue;
    ChannelStatistics alpha;
    ChannelStatistics luminance;   // Legacy Rec.709-weighted RGB values; no transfer decoding.
};

/// Logical RGBA sample read from a validated float16/float32 or normalized 8-bit view.
/// Values retain the source transfer function and alpha convention.
struct ImagePixelSample {
    std::array<float, 4> rgba{};
    int x = 0;
    int y = 0;
};

enum class ImageSpectrumChannel : std::uint8_t {
    Red,
    Green,
    Blue,
    Alpha,
    Rec709WeightedRgb,
};

/**
 * @brief 画像解析エンジン
 * 
 * RGBA float バッファ／画像 view からヒストグラム、統計情報、
 * ピクセルサンプル、空間周波数を算出し、自動露出や
 * ホワイトバランスの推定値も提供する。
 */
class LIBRARY_DLL_API ImageAnalyzer {
public:
    /// Reads one logical RGBA pixel from an explicitly described float16/float32 view.
    /// Does not allocate, convert color space, or un-premultiply alpha.
    static bool samplePixel(const ImageSurfaceView& image, int x, int y,
                            ImagePixelSample& sample) noexcept;
    /// Byte samples are normalized to 0..1; transfer and alpha conventions remain unchanged.
    static bool samplePixel(const ImageByteSurfaceView& image, int x, int y,
                            ImagePixelSample& sample) noexcept;

    /// Whole-image analysis; schedule outside frame-time critical paths.
    /// Computes the legacy 0..1 channel histograms and statistics directly from
    /// a float16/float32 or normalized 8-bit RGBA/BGRA view, honoring stride and
    /// channel order. Histogram values are clamped to [0, 1]; use samplePixel for
    /// unclamped HDR channel values.
    /// No full-image copy or color-space conversion is performed.
    static bool analyze(const ImageSurfaceView& image, ImageStatistics& statistics) noexcept;
    static bool analyze(const ImageByteSurfaceView& image,
                        ImageStatistics& statistics) noexcept;

    /// Explicit whole-image analysis (allocates OpenCV work buffers; not for a
    /// per-frame hot path). Computes a 2D spatial-frequency magnitude map of
    /// RGB, alpha, or Rec.709-weighted source RGB (without transfer decoding).
    /// DC is at (0, 0);
    /// bins are unshifted and normalized by the source pixel count. The caller
    /// owns the output buffer; count receives the required size on capacity failure.
    static bool analyzeSpatialFrequency(const ImageSurfaceView& image,
                                        ImageSpectrumChannel channel,
                                        float* magnitudes, std::size_t capacity,
                                        std::size_t& count);
    static bool analyzeSpatialFrequency(const ImageByteSurfaceView& image,
                                        ImageSpectrumChannel channel,
                                        float* magnitudes, std::size_t capacity,
                                        std::size_t& count);

    /// RGBA画像の全チャンネル統計を計算
    static ImageStatistics analyze(const float* pixels, int width, int height);

    /// 単一チャンネルの統計を計算
    /// @param channel 0=R, 1=G, 2=B, 3=A
    static ChannelStatistics analyzeChannel(const float* pixels, int width, int height,
                                             int channel);

    /// 輝度チャンネルの統計を計算 (Rec.709)
    static ChannelStatistics analyzeLuminance(const float* pixels, int width, int height);

    /// 自動露出補正値 (EV stops)
    /// 目標: 中間グレー (18%) に平均輝度を合わせる
    static float autoExposureEV(const float* pixels, int width, int height);

    /// 自動ホワイトバランス推定 (R, G, B の乗数を返す)
    /// Grey World仮定: 全ピクセルの平均が灰色になるべき
    static std::array<float, 3> autoWhiteBalance(const float* pixels, int width, int height);

    /// コントラスト比率 (最大輝度 / 最小輝度)
    static float contrastRatio(const float* pixels, int width, int height);

    /// ダイナミックレンジ (EV stops)
    static float dynamicRange(const float* pixels, int width, int height);

    /// 特定パーセンタイルの値を取得
    static float percentile(const ChannelStatistics& stats, float p);
};

} // namespace ArtifactCore
