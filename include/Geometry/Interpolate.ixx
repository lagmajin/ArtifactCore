module;

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module Math.Interpolate;

export namespace ArtifactCore {

using InterpFunc = std::function<float(float)>;

export enum class InterpolationType {
  // 基本形
  Linear,   // 線形補間
  Constant, // ステップ（補間なし）
  Smooth,   // 緩やかな補間（自動スムーズ）

  // 加速・減速系
  EaseIn,    // 徐々に加速
  EaseOut,   // 徐々に減速
  EaseInOut, // 加速→減速
  EaseOutIn, // 減速→加速（稀）

  // 二次・三次・指数系
  Quadratic,   // 二次曲線
  Cubic,       // 三次曲線
  Quartic,     // 四次
  Quintic,     // 五次
  Exponential, // 指数的
  Logarithmic, // 対数的

  // 円・三角・正弦
  Sine,     // 正弦波的
  Circular, // 円弧状（sqrt補間）
  Cosine,   // 余弦波補間（主にAudioやWaveで使用）

  // バウンス・弾性系
  BounceIn,     // 弾むように入る
  BounceOut,    // 弾むように出る
  BounceInOut,  // 両端で弾む
  ElasticIn,    // ゴムのように伸びて入る
  ElasticOut,   // ゴムのように出る
  ElasticInOut, // 両端で伸縮

  // バック・オーバーシュート系
  BackIn,    // 少し戻ってから進む
  BackOut,   // 行き過ぎて戻る
  BackInOut, // 双方向オーバーシュート

  // Bézier・スプライン系
  Bezier,      // 制御点によるBézier補間
  CatmullRom,  // Catmull-Romスプライン
  Hermite,     // Hermite補間（Tangents指定あり）
  Barycentric, // 三角補間（色空間やシェーディングで使用）

  // 物理系・特殊
  Spring,      // 物理スプリング感
  SmoothDamp,  // Unity系の減衰補間
  Step,        // 階段状
  CustomCurve, // ユーザー定義カーブ
  Polynomial,  // 任意多項式
  Sigmoid,     // S字カーブ（AI・グラデーション向き）

  // 色補間・HDR向け
  GammaCorrected, // ガンマ補正付き線形
  Perceptual,     // 人間知覚ベース補間（HDR / 色補間）
};

struct Linear {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    return start + (end - start) * alpha;
  }
};

// Generic linear interpolation function (LERP)
export template <typename T>
T lerp(const T &start, const T &end, double alpha) {
  return start + (end - start) * alpha;
}

// EaseIn補間
struct EaseIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = alpha * alpha;
    return start + (end - start) * alpha;
  }
};

// Back補間 (Overshoot)
struct BackIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    const float s = 1.70158f;
    return start + (end - start) * (alpha * alpha * ((s + 1.0f) * alpha - s));
  }
};

// Back補間 (Overshoot)
struct BackOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    const float s = 1.70158f;
    alpha = alpha - 1.0f;
    alpha = (alpha * alpha * ((s + 1.0f) * alpha + s) + 1.0f);
    return start + (end - start) * alpha;
  }
};

struct BackInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    const float s = 1.70158f * 1.525f;
    if (alpha < 0.5f) {
      const float u = alpha * 2.0f;
      alpha = 0.5f * (u * u * ((s + 1.0f) * u - s));
    } else {
      const float u = alpha * 2.0f - 2.0f;
      alpha = 0.5f * (u * u * ((s + 1.0f) * u + s) + 2.0f);
    }
    return start + (end - start) * alpha;
  }
};

// Bounce補間
struct BounceOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
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

struct BounceIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    return start + (end - start) * (1.0f - BounceOut()(0.0f, 1.0f, 1.0f - alpha));
  }
};

struct BounceInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    if (alpha < 0.5f) {
      const float u = 1.0f - 2.0f * alpha;
      return start + (end - start) * (0.5f * (1.0f - BounceOut()(0.0f, 1.0f, u)));
    }
    const float u = 2.0f * alpha - 1.0f;
    return start + (end - start) * (0.5f * BounceOut()(0.0f, 1.0f, u) + 0.5f);
  }
};

// Elastic補間 (Rubber band)
struct ElasticOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    if (alpha <= 0.0f)
      return start;
    if (alpha >= 1.0f)
      return end;
    const float p = 0.3f;
    const float s = p / 4.0f;
    float val = (std::pow(2.0f, -10.0f * alpha) *
                     std::sin((alpha - s) * (2.0f * 3.14159265f) / p) +
                 1.0f);
    return start + (end - start) * val;
  }
};

struct ElasticIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    if (alpha <= 0.0f)
      return start;
    if (alpha >= 1.0f)
      return end;
    const float p = 0.3f;
    const float s = p / 4.0f;
    float val = -std::pow(2.0f, 10.0f * (alpha - 1.0f)) *
                std::sin((alpha - 1.0f - s) * (2.0f * 3.14159265f) / p);
    return start + (end - start) * val;
  }
};

struct ElasticInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    if (alpha <= 0.0f)
      return start;
    if (alpha >= 1.0f)
      return end;
    const float p = 0.45f;
    const float s = p / 4.0f;
    float val;
    if (alpha < 0.5f) {
      const float x = alpha * 2.0f;
      val = -0.5f * std::pow(2.0f, 10.0f * (x - 1.0f)) *
            std::sin((x - 1.0f - s) * (2.0f * 3.14159265f) / p);
    } else {
      const float x = alpha * 2.0f - 1.0f;
      val = std::pow(2.0f, -10.0f * x) *
            std::sin((x - s) * (2.0f * 3.14159265f) / p) * 0.5f + 0.5f;
    }
    return start + (end - start) * val;
  }
};

struct EaseOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = 1.0f - std::pow(1.0f - alpha, 2.0f);
    return start + (end - start) * alpha;
  }
};

struct EaseInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    if (alpha < 0.5f) {
      alpha = 2.0f * alpha * alpha;
    } else {
      alpha = 1.0f - std::pow(-2.0f * alpha + 2.0f, 2.0f) / 2.0f;
    }
    return start + (end - start) * alpha;
  }
};

struct SineOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::sin((alpha * 3.14159265f) / 2.0f);
    return start + (end - start) * alpha;
  }
};

export inline float bezierEvaluate(float t, float cp1x, float cp1y,
                                   float cp2x, float cp2y) noexcept {
  // 3次ベジェ 数値解 Newton-Raphson法
  // AE 互換 誤差 1e-6 で 4回反復
  float x = t;
  for (int i = 0; i < 4; i++) {
    const float t2 = x * x;
    const float t3 = t2 * x;

    const float mt = 1.0f - x;
    const float mt2 = mt * mt;

    const float dx = 3.0f * mt2 * cp1x + 6.0f * mt * x * (cp2x - cp1x) +
                     3.0f * t2 * (1.0f - cp2x);

    if (std::abs(dx) < 1e-6f) {
      break;
    }

    const float cx = 3.0f * mt2 * x * cp1x + 3.0f * mt * t2 * cp2x + t3;
    x -= (cx - t) / dx;
  }

  const float t2 = x * x;
  const float t3 = t2 * x;
  const float mt = 1.0f - x;
  const float mt2 = mt * mt;

  return 3.0f * mt2 * x * cp1y + 3.0f * mt * t2 * cp2y + t3;
}

struct CubicOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = 1.0f - std::pow(1.0f - alpha, 3.0f);
    return start + (end - start) * alpha;
  }
};

struct CubicIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    alpha = alpha * alpha * alpha;
    return start + (end - start) * alpha;
  }
};

struct CubicInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha < 0.5f) {
      alpha = 4.0f * alpha * alpha * alpha;
    } else {
      alpha = 1.0f - std::pow(-2.0f * alpha + 2.0f, 3.0f) / 2.0f;
    }
    return start + (end - start) * alpha;
  }
};

struct QuarticIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    const float a2 = alpha * alpha;
    alpha = a2 * a2;
    return start + (end - start) * alpha;
  }
};

struct QuarticOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    alpha = 1.0f - std::pow(1.0f - alpha, 4.0f);
    return start + (end - start) * alpha;
  }
};

struct QuarticInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha < 0.5f) {
      const float a2 = alpha * alpha;
      alpha = 8.0f * a2 * a2;
    } else {
      alpha = 1.0f - std::pow(-2.0f * alpha + 2.0f, 4.0f) / 2.0f;
    }
    return start + (end - start) * alpha;
  }
};

struct QuinticIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    alpha = alpha * alpha * alpha * alpha * alpha;
    return start + (end - start) * alpha;
  }
};

struct QuinticOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    alpha = 1.0f - std::pow(1.0f - alpha, 5.0f);
    return start + (end - start) * alpha;
  }
};

struct QuinticInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha < 0.5f) {
      alpha = 16.0f * alpha * alpha * alpha * alpha * alpha;
    } else {
      alpha = 1.0f - std::pow(-2.0f * alpha + 2.0f, 5.0f) / 2.0f;
    }
    return start + (end - start) * alpha;
  }
};

struct SineIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    alpha = 1.0f - std::cos(alpha * 3.14159265f / 2.0f);
    return start + (end - start) * alpha;
  }
};

struct SineInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    alpha = -(std::cos(3.14159265f * alpha) - 1.0f) / 2.0f;
    return start + (end - start) * alpha;
  }
};

struct CosineEase {
  // Identical curve to SineInOut; named alias for the Cosine enum entry.
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    return SineInOut()(start, end, alpha);
  }
};

struct CircularIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    alpha = 1.0f - std::sqrt(std::max(0.0f, 1.0f - alpha * alpha));
    return start + (end - start) * alpha;
  }
};

struct CircularOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    alpha = std::sqrt(std::max(0.0f, 1.0f - (alpha - 1.0f) * (alpha - 1.0f)));
    return start + (end - start) * alpha;
  }
};

struct CircularInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha < 0.5f) {
      alpha = (1.0f - std::sqrt(std::max(0.0f, 1.0f - 4.0f * alpha * alpha))) / 2.0f;
    } else {
      alpha = (std::sqrt(std::max(0.0f, 1.0f - (-2.0f * alpha + 2.0f) * (-2.0f * alpha + 2.0f))) + 1.0f) / 2.0f;
    }
    return start + (end - start) * alpha;
  }
};

struct ExponentialIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha <= 0.0f) return start;
    alpha = std::pow(2.0f, 10.0f * alpha - 10.0f);
    return start + (end - start) * alpha;
  }
};

struct ExponentialOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha >= 1.0f) return end;
    alpha = 1.0f - std::pow(2.0f, -10.0f * alpha);
    return start + (end - start) * alpha;
  }
};

struct ExponentialInOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha <= 0.0f) return start;
    if (alpha >= 1.0f) return end;
    if (alpha < 0.5f) {
      alpha = std::pow(2.0f, 20.0f * alpha - 10.0f) / 2.0f;
    } else {
      alpha = (2.0f - std::pow(2.0f, -20.0f * alpha + 10.0f)) / 2.0f;
    }
    return start + (end - start) * alpha;
  }
};

struct LogarithmicEase {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    alpha = std::log(1.0f + 1.71828183f * alpha);
    return start + (end - start) * alpha;
  }
};

struct EaseOutIn {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha < 0.5f) {
      const float u = alpha * 2.0f;
      alpha = (1.0f - (1.0f - u) * (1.0f - u)) * 0.5f;
    } else {
      const float u = (alpha - 0.5f) * 2.0f;
      alpha = 0.5f + (u * u) * 0.5f;
    }
    return start + (end - start) * alpha;
  }
};

export template <typename T>
T interpolate(const T &start, const T &end, float alpha,
              InterpolationType type) {
  switch (type) {
  case InterpolationType::Constant:
    return (alpha < 1.0f) ? start : end;
  case InterpolationType::Linear:
    return Linear()(start, end, alpha);
  case InterpolationType::EaseIn:
    return EaseIn()(start, end, alpha);
  case InterpolationType::EaseOut:
    return EaseOut()(start, end, alpha);
  case InterpolationType::EaseInOut:
    return EaseInOut()(start, end, alpha);
  case InterpolationType::EaseOutIn:
    return EaseOutIn()(start, end, alpha);
  case InterpolationType::Smooth:
  case InterpolationType::Cosine:
    return CosineEase()(start, end, alpha);
  case InterpolationType::Quadratic:
    return EaseIn()(start, end, alpha);
  case InterpolationType::Cubic:
    return CubicOut()(start, end, alpha);
  case InterpolationType::BounceIn:
    return BounceIn()(start, end, alpha);
  case InterpolationType::BounceOut:
    return BounceOut()(start, end, alpha);
  case InterpolationType::BounceInOut:
    return BounceInOut()(start, end, alpha);
  case InterpolationType::ElasticIn:
    return ElasticIn()(start, end, alpha);
  case InterpolationType::ElasticOut:
    return ElasticOut()(start, end, alpha);
  case InterpolationType::ElasticInOut:
    return ElasticInOut()(start, end, alpha);
  case InterpolationType::BackIn:
    return BackIn()(start, end, alpha);
  case InterpolationType::BackOut:
    return BackOut()(start, end, alpha);
  case InterpolationType::BackInOut:
    return BackInOut()(start, end, alpha);
  case InterpolationType::Sine:
    return SineOut()(start, end, alpha);
  case InterpolationType::Quartic:
    return QuarticOut()(start, end, alpha);
  case InterpolationType::Quintic:
    return QuinticOut()(start, end, alpha);
  case InterpolationType::Exponential:
    return ExponentialOut()(start, end, alpha);
  case InterpolationType::Logarithmic:
    return LogarithmicEase()(start, end, alpha);
  case InterpolationType::Circular:
    return CircularOut()(start, end, alpha);
  case InterpolationType::Bezier:
    // Bezier needs control points — KeyframeInterpolator and the color path
    // route it to bezierInterpolate(); direct calls fall back to Linear.
  default:
    return Linear()(start, end, alpha);
  }
}

/**
 * @brief Bezier control points を使った補間
 *
 * @param start 始点値
 * @param end 終点値
 * @param alpha 時間パラメータ [0, 1]
 * @param cp1_x, cp1_y, cp2_x, cp2_y AE 互換のベジェ制御点
 */
export template <typename T>
T bezierInterpolate(const T &start, const T &end, float alpha,
                    float cp1_x, float cp1_y, float cp2_x, float cp2_y) {
  const float easedT = bezierEvaluate(alpha, cp1_x, cp1_y, cp2_x, cp2_y);
  return start + (end - start) * easedT;
}

/**
 * @brief Hermite補間 (接線指定)
 *
 * Tangents m0/m1 are in value units per unit alpha. Zero tangents give the
 * smoothstep curve. Out-of-range alpha extrapolates the polynomial as-is.
 */
export template <typename T>
T hermiteInterpolate(const T &p0, const T &m0, const T &p1, const T &m1,
                     float alpha) {
  const float t = alpha;
  const float t2 = t * t;
  const float t3 = t2 * t;
  const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
  const float h10 = t3 - 2.0f * t2 + t;
  const float h01 = -2.0f * t3 + 3.0f * t2;
  const float h11 = t3 - t2;
  return p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11;
}

/**
 * @brief 均一 Catmull-Romスプライン (p1→p2区間、両端は隣接点)
 *
 * Endpoints duplicate when neighbors are missing (evaluate() clamps).
 */
export template <typename T>
T catmullRomInterpolate(const T &p0, const T &p1, const T &p2, const T &p3,
                        float alpha) {
  const T m0 = (p2 - p0) * 0.5f;
  const T m1 = (p3 - p1) * 0.5f;
  return hermiteInterpolate(p1, m0, p2, m1, alpha);
}

/**
 * @brief Bezier カーブの速度（dy/dx）を計算
 *
 * speed graph 表示や微分値として使用。
 * 数値微分により dy/dx を求める。
 */
export inline float bezierSpeed(float t, float cp1x, float cp1y,
                                float cp2x, float cp2y,
                                float epsilon = 1e-4f) noexcept {
  const float y1 = bezierEvaluate(t, cp1x, cp1y, cp2x, cp2y);
  const float y2 = bezierEvaluate(t + epsilon, cp1x, cp1y, cp2x, cp2y);
  return (y2 - y1) / epsilon;
}

/**
 * @brief 複数キーフレーム間の補間を行うインターポレーター
 *
 * キーフレームのリストから現在時刻に対応する値を補間する。
 * Bezier 制御点に対応し、speed graph の計算も可能。
 */
export template <typename T>
class KeyframeInterpolator {
public:
  struct KeyframeEntry {
    double time = 0.0;
    T value{};
    InterpolationType type = InterpolationType::Linear;
    float cp1_x = 0.42f, cp1_y = 0.0f;
    float cp2_x = 0.58f, cp2_y = 1.0f;
  };

  void clear() { keyframes_.clear(); }

  void addKeyframe(const KeyframeEntry& kf) {
    keyframes_.push_back(kf);
    std::sort(keyframes_.begin(), keyframes_.end(),
              [](const KeyframeEntry& a, const KeyframeEntry& b) {
                return a.time < b.time;
              });
  }

  T evaluate(double time) const {
    if (keyframes_.empty()) return T{};
    if (time <= keyframes_.front().time) return keyframes_.front().value;
    if (time >= keyframes_.back().time) return keyframes_.back().value;

    auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), time,
      [](const KeyframeEntry& a, double t) { return a.time < t; });

    if (it == keyframes_.begin()) return keyframes_.front().value;

    const std::size_t i = static_cast<std::size_t>(it - keyframes_.begin());
    const auto& prev = keyframes_[i - 1];
    const auto& curr = keyframes_[i];

    if (prev.type == InterpolationType::Constant) return prev.value;

    const double duration = curr.time - prev.time;
    if (duration <= 0.0) return prev.value;

    const float alpha = static_cast<float>((time - prev.time) / duration);

    if (prev.type == InterpolationType::Bezier) {
      return bezierInterpolate(prev.value, curr.value, alpha,
                               prev.cp1_x, prev.cp1_y, prev.cp2_x, prev.cp2_y);
    }

    if (prev.type == InterpolationType::CatmullRom ||
        prev.type == InterpolationType::Hermite) {
      const auto& before = keyframes_[i >= 2 ? i - 2 : 0];
      const auto& after = keyframes_[std::min(i + 1, keyframes_.size() - 1)];
      if (prev.type == InterpolationType::CatmullRom) {
        // Uniform Catmull-Rom over values; edge keys duplicate.
        return catmullRomInterpolate(before.value, prev.value, curr.value,
                                     after.value, alpha);
      }
      // Hermite with finite-difference tangents scaled by this segment's
      // duration (matches Catmull-Rom for uniformly spaced keys).
      const double beforeSpan = curr.time - before.time;
      const double afterSpan = after.time - prev.time;
      T m0 = prev.value;
      T m1 = curr.value;
      if (beforeSpan > 0.0) {
        m0 = (curr.value - before.value) *
             static_cast<float>(duration / beforeSpan);
      } else {
        m0 = curr.value - prev.value;
      }
      if (afterSpan > 0.0) {
        m1 = (after.value - prev.value) *
             static_cast<float>(duration / afterSpan);
      } else {
        m1 = curr.value - prev.value;
      }
      return hermiteInterpolate(prev.value, m0, curr.value, m1, alpha);
    }

    return interpolate(prev.value, curr.value, alpha, prev.type);
  }

  float speedAt(double time) const {
    if (keyframes_.size() < 2) return 0.0f;
    const float epsilon = 1e-4f;
    const float v1 = evaluate(time);
    const float v2 = evaluate(time + epsilon);
    return (v2 - v1) / epsilon;
  }

  std::vector<KeyframeEntry> keyframes() const { return keyframes_; }
  bool isEmpty() const { return keyframes_.empty(); }
  size_t size() const { return keyframes_.size(); }

private:
  std::vector<KeyframeEntry> keyframes_;
};

}; // namespace ArtifactCore
