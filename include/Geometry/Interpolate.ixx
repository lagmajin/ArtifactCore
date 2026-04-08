module;

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
export module Math.Interpolate;





export namespace ArtifactCore {
 
 using InterpFunc = std::function<float(float)>;

 export enum class InterpolationType {
  // 基本形
  Linear,          // 線形補間
  Constant,        // ステップ（補間なし）
  Smooth,          // 緩やかな補間（自動スムーズ）

  // 加速・減速系
  EaseIn,          // 徐々に加速
  EaseOut,         // 徐々に減速
  EaseInOut,       // 加速→減速
  EaseOutIn,       // 減速→加速（稀）

  // 二次・三次・指数系
  Quadratic,       // 二次曲線
  Cubic,           // 三次曲線
  Quartic,         // 四次
  Quintic,         // 五次
  Exponential,     // 指数的
  Logarithmic,     // 対数的

  // 円・三角・正弦
  Sine,            // 正弦波的
  Circular,        // 円弧状（sqrt補間）
  Cosine,          // 余弦波補間（主にAudioやWaveで使用）

  // バウンス・弾性系
  BounceIn,        // 弾むように入る
  BounceOut,       // 弾むように出る
  BounceInOut,     // 両端で弾む
  ElasticIn,       // ゴムのように伸びて入る
  ElasticOut,      // ゴムのように出る
  ElasticInOut,    // 両端で伸縮

  // バック・オーバーシュート系
  BackIn,          // 少し戻ってから進む
  BackOut,         // 行き過ぎて戻る
  BackInOut,       // 双方向オーバーシュート

  // Bézier・スプライン系
  Bezier,          // 制御点によるBézier補間
  CatmullRom,      // Catmull-Romスプライン
  Hermite,         // Hermite補間（Tangents指定あり）
  Barycentric,     // 三角補間（色空間やシェーディングで使用）

  // 物理系・特殊
  Spring,          // 物理スプリング感
  SmoothDamp,      // Unity系の減衰補間
  Step,            // 階段状
  CustomCurve,     // ユーザー定義カーブ
  Polynomial,      // 任意多項式
  Sigmoid,         // S字カーブ（AI・グラデーション向き）

  // 色補間・HDR向け
  GammaCorrected,  // ガンマ補正付き線形
  Perceptual,      // 人間知覚ベース補間（HDR / 色補間）
 };

 struct Linear {
  template<typename T>
  T operator()(const T& start, const T& end, float alpha) const {
   return start + (end - start) * alpha;
  }
 };

 // Generic linear interpolation function (LERP)
 export template<typename T>
 T lerp(const T& start, const T& end, double alpha) {
  return start + (end - start) * alpha;
 }

 // EaseIn補間
 struct EaseIn {
  template<typename T>
  T operator()(const T& start, const T& end, float alpha) const {
   alpha = alpha * alpha;
   return start + (end - start) * alpha;
  }
 };

 // Back補間 (Overshoot)
 struct BackOut {
  template<typename T>
  T operator()(const T& start, const T& end, float alpha) const {
   const float s = 1.70158f;
   alpha = alpha - 1.0f;
   alpha = (alpha * alpha * ((s + 1.0f) * alpha + s) + 1.0f);
   return start + (end - start) * alpha;
  }
 };

 // Bounce補間
 struct BounceOut {
  template<typename T>
  T operator()(const T& start, const T& end, float alpha) const {
   if (alpha < (1.0f / 2.75f)) {
    alpha = (7.5625f * alpha * alpha);
   } else if (alpha < (2.0f / 2.75f)) {
    alpha -= (1.5f / 2.75f);
    alpha = (7.5625f * alpha * alpha + 0.75f);
   } else if (alpha < (2.5f / 2.75f)) {
    alpha -= (2.25f / 2.75f);
    alpha = (7.5625f * alpha * alpha + 0.9375f);
   } else {
    alpha -= (2.625f / 2.75f);
    alpha = (7.5625f * alpha * alpha + 0.984375f);
   }
   return start + (end - start) * alpha;
  }
 };

 // Elastic補間 (Rubber band)
 struct ElasticOut {
  template<typename T>
  T operator()(const T& start, const T& end, float alpha) const {
   if (alpha <= 0.0f) return start;
   if (alpha >= 1.0f) return end;
   const float p = 0.3f;
   const float s = p / 4.0f;
   float val = (std::pow(2.0f, -10.0f * alpha) * std::sin((alpha - s) * (2.0f * 3.14159265f) / p) + 1.0f);
   return start + (end - start) * val;
  }
 };

 export template<typename T>
 T interpolate(const T& start, const T& end, float alpha, InterpolationType type) {
  switch (type) {
   case InterpolationType::Constant: return (alpha < 1.0f) ? start : end;
   case InterpolationType::EaseIn: return EaseIn()(start, end, alpha);
   case InterpolationType::BackOut: return BackOut()(start, end, alpha);
   case InterpolationType::BounceOut: return BounceOut()(start, end, alpha);
   case InterpolationType::ElasticOut: return ElasticOut()(start, end, alpha);
   case InterpolationType::Linear:
   default:
    return start + (end - start) * alpha;
  }
 }










};