module;

#include "../Define/DllExportMacro.hpp"
#include <array>

export module Analyze.Histogram;

import std;

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
    ChannelStatistics luminance;   // Rec.709 輝度
};

/**
 * @brief 画像解析エンジン
 * 
 * RGBA float バッファからヒストグラム、統計情報、
 * 自動露出/ホワイトバランス推定値を算出する。
 */
class LIBRARY_DLL_API ImageAnalyzer {
public:
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
