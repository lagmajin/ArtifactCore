module;

#include "../Define/DllExportMacro.hpp"

export module Generator:Noise;

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

/**
 * @brief ノイズベースの画像生成
 * 
 * NoiseGenerator (Perlin/Worley/Fractal) を使って
 * RGBA float バッファにノイズテクスチャを生成する。
 */
class LIBRARY_DLL_API NoiseImageGenerator {
public:
    // --- Perlin Noise ---

    /// Perlinノイズ画像 (グレースケール)
    /// @param scale  周波数スケール (大きい=細かい)
    /// @param offsetX, offsetY  ノイズ空間のオフセット（アニメーション用）
    static void perlinNoise(float* pixels, int width, int height,
                            float scale = 10.0f,
                            float offsetX = 0.0f, float offsetY = 0.0f);

    /// カラーPerlinノイズ (各チャンネルで異なるオフセット)
    static void perlinNoiseColor(float* pixels, int width, int height,
                                  float scale = 10.0f);

    // --- Fractal Noise (fBm) ---

    /// フラクタルノイズ画像
    /// @param octaves    重ね合わせ層数 (4-8が一般的)
    /// @param persistence 各層の振幅減衰 (0.5がデフォルト)
    /// @param lacunarity  各層の周波数倍率 (2.0がデフォルト)
    static void fractalNoise(float* pixels, int width, int height,
                              float scale = 10.0f, int octaves = 6,
                              float persistence = 0.5f, float lacunarity = 2.0f);

    // --- Worley Noise (Voronoi) ---

    /// Worleyノイズ画像 (セル状テクスチャ)
    static void worleyNoise(float* pixels, int width, int height,
                             float scale = 5.0f);

    // --- Turbulence ---

    /// タービュランスノイズ (|perlin| の fBm — 雲/煙テクスチャ用)
    static void turbulence(float* pixels, int width, int height,
                            float scale = 8.0f, int octaves = 6);

    // --- Specialty ---

    /// 雲テクスチャ (フラクタル + コントラスト調整)
    static void cloudTexture(float* pixels, int width, int height,
                              float scale = 6.0f, float coverage = 0.5f);

    /// ウッドグレイン (同心円状のPerlinノイズ)
    static void woodGrain(float* pixels, int width, int height,
                           float scale = 20.0f, float ringFrequency = 10.0f);

    /// マーブル (歪んだストライプ)
    static void marble(float* pixels, int width, int height,
                        float scale = 5.0f, float stripeFrequency = 5.0f);
};

} // namespace ArtifactCore