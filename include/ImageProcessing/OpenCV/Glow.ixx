module;
#include <utility>
#include <random>
#include <array>
#include <cstdint>
#include "../../Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
export module Glow;

export namespace ArtifactCore {

 enum class SoftKneeGlowAlphaPolicy : std::uint8_t {
  PreserveSourceCoverage,
  ExpandFromGlowEnergy,
 };

 struct SoftKneeGlowParameters {
  float threshold = 0.45f;
  float knee = 0.30f;
  float amount = 1.45f;
  float radius = 8.0f;
  float alphaExpansionGain = 20.0f;
  std::array<float, 4> weights{0.48f, 0.30f, 0.15f, 0.07f};
  std::array<float, 4> scales{0.72f, 1.8f, 4.2f, 9.0f};
  std::array<float, 3> tint{1.0f, 0.92f, 0.82f};
  SoftKneeGlowAlphaPolicy alphaPolicy =
      SoftKneeGlowAlphaPolicy::ExpandFromGlowEnergy;
 };

 // Caller-owned fixed-capacity scratch. Keep it with the effect instance so
 // steady-size frames reuse pyramid and accumulation storage.
 struct SoftKneeGlowWorkspace {
  cv::Mat fullResolutionSeedAndUpsample;
  std::array<cv::Mat, 2> downsampleSeeds;
  cv::Mat bloom;
 };

 enum class LightLeakLook : std::uint8_t {
  EdgeWash,
  AnamorphicStreak,
  OrganicGateLeak,
 };

 enum class LightLeakAlphaPolicy : std::uint8_t {
  PreserveSourceCoverage,
  ClipLeakToSourceCoverage,
  ExpandFromLeakEnergy,
 };

 struct LightLeakParameters {
  LightLeakLook look = LightLeakLook::EdgeWash;
  LightLeakAlphaPolicy alphaPolicy = LightLeakAlphaPolicy::PreserveSourceCoverage;
  float strength = 0.55f;
  // Normalized source location used by the anamorphic streak look only.
  float streakSourceX = 0.01f;
  float streakSourceY = 0.105f;
  // Streak-axis angle in degrees, clamped to [-45, 45].
  float streakAngleDegrees = 2.5783101f;
  float alphaExpansionGain = 1.1f;
  float alphaExpansionLimit = 0.82f;
 };

 // Analytic still-image leak field. Source and destination are CV_32FC4 RGBA,
 // linear-light, premultiplied surfaces. No blur pyramid or temporary image is
 // allocated by the pixel loop; output remains scene-linear and unclipped.
 LIBRARY_DLL_API bool applyStillLightLeak(
     const cv::Mat& sourceLinearPremultipliedRgba,
     cv::Mat& destinationLinearPremultipliedRgba,
     const LightLeakParameters& parameters = {});

 // Input and output are CV_32FC4 RGBA, linear-light, premultiplied surfaces.
 // The output is scene-linear; no display-referred clipping is applied.
 LIBRARY_DLL_API bool applySoftKneeMultiScaleGlow(
     const cv::Mat& sourceLinearPremultipliedRgba,
     cv::Mat& destinationLinearPremultipliedRgba,
     SoftKneeGlowWorkspace& workspace,
     const SoftKneeGlowParameters& parameters = {});

 LIBRARY_DLL_API void applySimpleGlow(
  const cv::Mat& src,               // 入力画像（BGR or BGRA）
  const cv::Mat& mask,              // グロー対象マスク（CV_8UC1 or empty）
  cv::Mat& dst,                     // 出力先

  const cv::Scalar& glowColor,      // グロー色（例：白 / 任意色）
  float glowGain = 1.0f,            // 全体グロー強度係数

  int layerCount = 4,               // 重ねるレイヤ数（例：4）
  float baseSigma = 5.0f,           // 最初のブラー半径
  float sigmaGrowth = 1.8f,         // ブラー半径の増加比
  float baseAlpha = 0.3f,           // 最初の合成比率
  float alphaFalloff = 0.6f,        // 合成比の減衰（指数的）

  bool additiveBlend = true,        // 合成するか否か（src + glow）
  bool linearSpace = true           // 線形色空間で処理
 );

 LIBRARY_DLL_API cv::Mat applyGlintGlow(const cv::Mat& src, float threshold_value, int blur_radius, float intensity);


 LIBRARY_DLL_API void drawRandomDottedVerticalLine(cv::Mat& image, int x_center, int line_width,
  long num_points_to_try, unsigned int seed,
  const cv::Scalar& point_color, int point_size);


 LIBRARY_DLL_API cv::Mat applyVerticalGlow(const cv::Mat& src, float threshold_value, int vertical_blur_radius, float intensity);

}
