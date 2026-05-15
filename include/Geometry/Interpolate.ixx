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
struct BackOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    const float s = 1.70158f;
    alpha = alpha - 1.0f;
    alpha = (alpha * alpha * ((s + 1.0f) * alpha + s) + 1.0f);
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
    const float mt3 = mt2 * mt;

    const float dx = 3.0f * mt2 * cp1x + 6.0f * mt * x * (cp2x - cp1x) +
                     3.0f * t2 * (1.0f - cp2x);

    if (std::abs(dx) < 1e-6f) {
      break;
    }

    const float cx = mt3 + 3.0f * mt2 * x * cp1x + 3.0f * mt * t2 * cp2x;
    x -= (cx - t) / dx;
  }

  const float t2 = x * x;
  const float t3 = t2 * x;
  const float mt = 1.0f - x;
  const float mt2 = mt * mt;
  const float mt3 = mt2 * mt;

  return mt3 + 3.0f * mt2 * x * cp1y + 3.0f * mt * t2 * cp2y + t3;
}

struct CubicOut {
  template <typename T>
  T operator()(const T &start, const T &end, float alpha) const {
    alpha = 1.0f - std::pow(1.0f - alpha, 3.0f);
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
  case InterpolationType::Sine:
    return SineOut()(start, end, alpha);
  case InterpolationType::Cubic:
    return CubicOut()(start, end, alpha);
  case InterpolationType::BackOut:
    return BackOut()(start, end, alpha);
  case InterpolationType::BounceOut:
    return BounceOut()(start, end, alpha);
  case InterpolationType::ElasticOut:
    return ElasticOut()(start, end, alpha);
  case InterpolationType::Bezier:
    return start; // bezier requires control points — use bezierInterpolate()
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

    const auto& prev = *(it - 1);
    const auto& curr = *it;

    if (prev.type == InterpolationType::Constant) return prev.value;

    const double duration = curr.time - prev.time;
    if (duration <= 0.0) return prev.value;

    const float alpha = static_cast<float>((time - prev.time) / duration);

    if (prev.type == InterpolationType::Bezier) {
      return bezierInterpolate(prev.value, curr.value, alpha,
                               prev.cp1_x, prev.cp1_y, prev.cp2_x, prev.cp2_y);
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

  const std::vector<KeyframeEntry>& keyframes() const { return keyframes_; }
  bool isEmpty() const { return keyframes_.empty(); }
  size_t size() const { return keyframes_.size(); }

private:
  std::vector<KeyframeEntry> keyframes_;
};

}; // namespace ArtifactCore
