module;

#include "../Define/DllExportMacro.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <array>
#include <random>
#include <chrono>
#include <type_traits>
#include <tuple>
#include <numeric>

export module StarfieldGenerator;

export namespace ArtifactCore {

/**
 * @brief 星のスペクトル型
 * 
 * モーガン・キーナン分類 (OBAFGKM)
 */
enum class SpectralType : int {
    O = 0,  // 青白・高温 (>30,000K)
    B = 1,  // 青白 (10,000-30,000K)
    A = 2,  // 白 (7,500-10,000K)
    F = 3,  // 黄白 (6,000-7,500K)
    G = 4,  // 黄 (5,000-6,000K) - 太陽はG2V
    K = 5,  // 橙 (3,500-5,000K)
    M = 6,  // 赤 (<3,500K)
    Count = 7
};

/**
 * @brief 星の光度階級
 * 
 * Ia: 輝超巨星, Ib: 超巨星, II: 輝巨星,
 * III: 巨星, IV: 準巨星, V: 主系列星 (太陽),
 * VI: 準矮星, VII: 白色矮星
 */
enum class LuminosityClass : int {
    Ia  = 0,  // 輝超巨星
    Ib  = 1,  // 超巨星
    II  = 2,  // 輝巨星
    III = 3,  // 巨星
    IV  = 4,  // 準巨星
    V   = 5,  // 主系列星
    VI  = 6,  // 準矮星
    VII = 7,  // 白色矮星
    Count = 8
};

/**
 * @brief 単一の星データ
 */
struct LIBRARY_DLL_API StarData {
    float x = 0.0f;          float y = 0.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f;
    float brightness = 1.0f; float size = 1.0f;
    bool  hasGlare = false;
    float twinkleSpeed = 0.0f, twinklePhase = 0.0f;
};

/**
 * @brief 星雲データ
 */
struct LIBRARY_DLL_API NebulaData {
    float centerX = 0.5f, centerY = 0.5f;
    float radiusX = 0.3f, radiusY = 0.2f;
    float rotation = 0.0f;
    float r = 0.2f, g = 0.1f, b = 0.3f;
    float opacity = 0.5f;
    /// 星雲の種類: 0=発光星雲(HII), 1=反射星雲, 2=超新星残骸, 3=惑星状星雲
    int nebulaType = 0;
};

/**
 * @brief 惑星データ
 */
struct LIBRARY_DLL_API PlanetData {
    float orbitRadius = 0.1f;     // 軌道半径 (画面幅に対する比率)
    float orbitPhase = 0.0f;      // 初期位相 (0-2π)
    float orbitSpeed = 1.0f;      // 公転速度 (rad/s, 負で逆回転)
    float eccentricity = 0.0f;    // 離心率 (0=円軌道)
    float planetRadius = 0.005f;  // 惑星半径 (画面幅比率)
    float r = 0.7f, g = 0.7f, b = 0.7f; // 表面色
    bool hasAtmosphere = false;
    float atmosphereR = 0.4f, atmosphereG = 0.6f, atmosphereB = 1.0f;
    bool hasRing = false;
    float ringInner = 1.3f;       // リング内径 (planetRadius 比)
    float ringOuter = 2.0f;       // リング外径
    float ringR = 0.9f, ringG = 0.85f, ringB = 0.7f;
};

/**
 * @brief 星系 (恒星+惑星系) データ
 */
struct LIBRARY_DLL_API StarSystemData {
    float centerX = 0.5f, centerY = 0.5f; // 星系の中心位置
    StarData star;                          // 中心星
    std::vector<PlanetData> planets;        // 惑星一覧
    bool showOrbits = false;                // 軌道線を表示
    float orbitLineAlpha = 0.15f;           // 軌道線の透明度
};

/**
 * @brief グレアパターン種類
 */
enum class GlarePattern : int {
    Cross4  = 0,  // 4条クロス (標準)
    Cross6  = 1,  // 6条 (六角絞り)
    Cross8  = 2,  // 8条 (八角絞り)
    Starburst = 3, // ランダム多芒
    Count = 4
};

/**
 * @brief H-R図に基づく宇宙背景生成器
 * 
 * 恒星統計・星雲・惑星系・銀河の生成を統合し、
 * 単一の float4 RGBA バッファに出力する。
 */
class LIBRARY_DLL_API StarfieldGenerator {
public:
    StarfieldGenerator();

    // ---------- 基本設定 ----------
    void setStarCount(int count);
    void setResolution(int width, int height);
    void setSeed(unsigned int seed);
    void setMilkyWayIntensity(float intensity);
    void setGlareEnabled(bool enabled);
    void setGlareThreshold(float threshold);
    /** グレアのパターンを設定 (default: Cross4) */
    void setGlarePattern(GlarePattern pattern);
    void setShootingStarChance(float chance);
    /** 背景の暗さ (0=真っ黒, 1=やや明るい. default: 0.0) */
    void setBackgroundLevel(float level);

    // ---------- 星雲 ----------
    void addNebula(const NebulaData& nebula);
    void clearNebulae();
    void setAutoNebulaCount(int count);

    // ---------- 銀河 ----------
    /** 背景に渦巻銀河を描画するか */
    void setGalaxyEnabled(bool enabled);
    /** 銀河の中心位置 */
    void setGalaxyCenter(float x, float y);
    /** 銀河の大きさ (画面比) */
    void setGalaxyRadius(float radius);
    /** 銀河の渦巻きの腕の数 */
    void setGalaxyArmCount(int count);
    /** 銀河の傾き (rad) */
    void setGalaxyTilt(float tilt);

    // ---------- 星系 ----------
    /** 星系を追加 (中心星+惑星) */
    void addStarSystem(const StarSystemData& system);
    void clearStarSystems();

    // ---------- 球状星団 ----------
    /** 球状星団の数を設定 (default: 0) */
    void setGlobularClusterCount(int count);

    // ---------- 星座線 ----------
    /** 星座線を有効化 (明るい星を結ぶ) */
    void setConstellationLinesEnabled(bool enabled);
    /** 星座線で結ぶ星の最低輝度 */
    void setConstellationLineThreshold(float threshold);
    /** 星座線の最大距離 (画面比) */
    void setConstellationLineMaxDist(float dist);

    // ---------- 生成 ----------
    void generate(float* pixels, float time = 0.0f);
    const std::vector<StarData>& getStars() const { return stars_; }
    const std::vector<StarSystemData>& getStarSystems() const { return systems_; }

    // ---------- ユーティリティ ----------
    static void spectralToRgb(SpectralType type, LuminosityClass lc,
                              float& r, float& g, float& b);
    static float spectralTemperature(SpectralType type, LuminosityClass lc);
    static float absoluteMagnitude(SpectralType type, LuminosityClass lc);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    std::vector<StarData> stars_;
    std::vector<NebulaData> nebulae_;
    std::vector<StarSystemData> systems_;
    int width_ = 1920, height_ = 1080;
    float milkyWayIntensity_ = 0.3f;
    bool glareEnabled_ = true;
    float glareThreshold_ = 0.6f;
    GlarePattern glarePattern_ = GlarePattern::Cross4;
    float shootingStarChance_ = 0.0f;
    float backgroundLevel_ = 0.0f;
    bool galaxyEnabled_ = false;
    float galaxyCenterX_ = 0.5f, galaxyCenterY_ = 0.5f;
    float galaxyRadius_ = 0.4f;
    int galaxyArmCount_ = 4;
    float galaxyTilt_ = 0.3f;
    int globularClusterCount_ = 0;
    bool constellationLines_ = false;
    float constellationThreshold_ = 0.5f;
    float constellationMaxDist_ = 0.15f;

    void generateStarDistribution(unsigned int seed);
    void renderStars(float* pixels, float time);
    void renderNebulae(float* pixels);
    void renderGalaxy(float* pixels);
    void renderStarSystems(float* pixels, float time);
    void renderGlobularClusters(float* pixels, float time);
    void renderShootingStars(float* pixels, float time);
    void renderConstellationLines(float* pixels);

    void applyGlare(float* pixels, int x, int y, float brightness,
                    float r, float g, float b);
    void drawDisc(float* pixels, float cx, float cy, float radius,
                  float r, float g, float b, float alpha);
    void drawRing(float* pixels, float cx, float cy,
                  float innerR, float outerR,
                  float r, float g, float b, float alpha,
                  float startAngle = 0.0f, float endAngle = 6.283185f);
};

} // namespace ArtifactCore
