module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <type_traits>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
export module Animation.Value;

import Frame.Position;
import Math.Interpolate;
import Container.NamedVector;


export namespace ArtifactCore {

// �w���p�[�֐��F���`��Ԃ̌W���v�Z
inline float calculateT(const FramePosition& start, const FramePosition& end, const FramePosition& current) {
 std::int64_t startFrame = start.framePosition();
 std::int64_t endFrame = end.framePosition();
 std::int64_t currentFrame = current.framePosition();
  
 if (endFrame <= startFrame) return 0.0f;
  
 float range = static_cast<float>(endFrame - startFrame);
 float offset = static_cast<float>(currentFrame - startFrame);
 return offset / range;
}

// �w���p�[�֐��F���`��ԁilerp�j
template<typename T>
inline T mix(const T& a, const T& b, float t) {
 return static_cast<T>(a + (b - a) * t);
}

template<typename T>
struct KeyFrameT {
 FramePosition frame;
 T value;
 InterpolationType interpolation = static_cast<InterpolationType>(0);
 // AE-compatible Bezier handles (x in time, y in value). Ignored unless
 // interpolation == Bezier. Out-of-range x is allowed (AE parity).
 float cp1_x = 0.42f, cp1_y = 0.0f;
 float cp2_x = 0.58f, cp2_y = 1.0f;
};

// 正規の easing 表は Math.Interpolate 側が所有する。ここでは int 直打ちの
// 二重テーブルを持たず、eased alpha が必要な互換用途向けに 0→1 補間で求める。
// (CatmullRom/Hermite 等の隣接点を要する型は Linear 落ち。Spline の厳密評価は
//  KeyframeInterpolator 側を使うこと。Bezier は at() が保持ハンドルで評価する。)
inline float interpolationAlpha(float alpha, InterpolationType type) {
  return interpolate(0.0f, 1.0f, alpha, type);
}

template<typename T>
inline T interpolateValue(const T& start, const T& end, float alpha, InterpolationType type) {
  return interpolate(start, end, alpha, type);
}

// 物理演算用のランタイム状態
export struct SpringState {
    float velocity = 0.0f;
    float stiffness = 120.0f;   // k
    float damping = 12.0f;      // c
    float mass = 1.0f;          // m
    float currentValue = 0.0f;  // 現在のシミュレーション位置
    bool initialized = false;
};

// =========================
// AnimatableValueT<T>
// =========================
 template<typename T>
 class AnimatableValueT {
 private:
  NamedVector<KeyFrameT<T>> keyframes_{
    makeNamedVector<KeyFrameT<T>>(ContainerName{"AnimatableValueKeyframes"})};
  T currentValue_{};
  mutable std::shared_mutex mutex_;
 public:
  AnimatableValueT() = default;
  explicit AnimatableValueT(const T& initial) : currentValue_(initial) {}
  AnimatableValueT(const AnimatableValueT& other) {
   std::shared_lock lock(other.mutex_);
   keyframes_ = other.keyframes_;
   currentValue_ = other.currentValue_;
  }
  AnimatableValueT& operator=(const AnimatableValueT& other) {
   if (this == &other) return *this;
   std::scoped_lock lock(mutex_, other.mutex_);
   keyframes_ = other.keyframes_;
   currentValue_ = other.currentValue_;
   return *this;
  }
  AnimatableValueT(AnimatableValueT&& other) noexcept {
   std::unique_lock lock(other.mutex_);
   keyframes_ = std::move(other.keyframes_);
   currentValue_ = std::move(other.currentValue_);
  }
  AnimatableValueT& operator=(AnimatableValueT&& other) noexcept {
   if (this == &other) return *this;
   std::scoped_lock lock(mutex_, other.mutex_);
   keyframes_ = std::move(other.keyframes_);
   currentValue_ = std::move(other.currentValue_);
   return *this;
  }

  // ݒliL[t[Ȃj
  void setCurrent(const T& v) {
   std::unique_lock lock(mutex_);
   currentValue_ = v;
  }

  T current() const {
   std::shared_lock lock(mutex_);
   return currentValue_;
  }

  T at(const FramePosition& frame) const {
   std::shared_lock lock(mutex_);
   if (keyframes_.isEmpty()) return currentValue_; // NamedVector API
   if (keyframes_.size() == 1) return keyframes_.at(0)->value;

   // 1. wt[SL[t[Oォ`FbN
   if (frame <= keyframes_.at(0)->frame) return keyframes_.at(0)->value;
   if (frame >= keyframes_.at(keyframes_.size() - 1)->frame) {
    return keyframes_.at(keyframes_.size() - 1)->value;
   }

   // 2. Binary search. Evaluation is const and deliberately has no mutable
   // cache: these values are evaluated concurrently by UI and render paths.
   size_t n = keyframes_.size();
   auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), frame,
 [](const auto& kf, const auto& f) { return kf.frame < f; });

   auto next = it;
   auto prev = std::prev(it);
   float t = calculateT(prev->frame, next->frame, frame);
   if (prev->interpolation == InterpolationType::Bezier) {
    const float cp1x = std::isfinite(prev->cp1_x) ? prev->cp1_x : 0.42f;
    const float cp1y = std::isfinite(prev->cp1_y) ? prev->cp1_y : 0.0f;
    const float cp2x = std::isfinite(prev->cp2_x) ? prev->cp2_x : 0.58f;
    const float cp2y = std::isfinite(prev->cp2_y) ? prev->cp2_y : 1.0f;
    return bezierInterpolate(prev->value, next->value, t, cp1x, cp1y, cp2x, cp2y);
   }
   return interpolateValue(prev->value, next->value, t, prev->interpolation);
  }

  // 物理ベースの評価 (Spring-Damper)
  // NOTE: float channels only. SpringState carries scalar state, so this
  // cannot represent T=Color/Transform/etc. Instantiating it for non-float
  // T is a compile error by design (use per-channel float values instead).
  float atSpring(const FramePosition& frame, float dt, SpringState& state) const {
      static_assert(std::is_same_v<T, float>,
          "AnimatableValueT::atSpring supports float channels only");
      float target = static_cast<float>(at(frame));
      if (!std::isfinite(target)) return state.currentValue;
      if (!state.initialized) {
          state.currentValue = target;
          state.velocity = 0.0f;
          state.initialized = true;
          return target;
      }

      // Semi-implicit Euler integration
      const float safeDt = std::isfinite(dt) ? std::clamp(dt, 0.0001f, 0.1f) : 0.0001f;
      const float safeStiffness = std::isfinite(state.stiffness) ? state.stiffness : 0.0f;
      const float safeDamping = std::isfinite(state.damping) ? state.damping : 0.0f;
      float force = -safeStiffness * (state.currentValue - target) - safeDamping * state.velocity;
      const float safeMass = std::isfinite(state.mass)
          ? std::max(std::abs(state.mass), 0.0001f) : 1.0f;
      state.velocity += (force / safeMass) * safeDt;
      state.currentValue += state.velocity * safeDt;

      return state.currentValue;
  }

  // L[t[ǉ
  void normalizeKeyFrames() {
   std::sort(keyframes_.begin(), keyframes_.end(),
    [](const auto& a, const auto& b) { return a.frame < b.frame; });
   for (std::size_t index = keyframes_.size(); index > 1;) {
    --index;
    if (keyframes_.at(index)->frame == keyframes_.at(index - 1)->frame) {
     keyframes_.takeAt(index);
    }
   }
  }

  void addKeyFrame(const FramePosition& frame, const T& value) {
   std::unique_lock lock(mutex_);
   // Keep the same invariant as AbstractProperty: one keyframe per time.
   // Replacing in place also preserves the existing interpolation mode
   // and Bezier handles (same as AbstractProperty's value-only update).
   auto existing = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (existing != keyframes_.end()) {
    existing->value = value;
    return;
   }
   keyframes_.append({ frame, value });
   normalizeKeyFrames();
  }

  void addKeyFrame(const FramePosition& frame, const T& value,
                   InterpolationType interpolation,
                   float cp1_x, float cp1_y, float cp2_x, float cp2_y) {
   std::unique_lock lock(mutex_);
   auto existing = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (existing != keyframes_.end()) {
    existing->value = value;
    existing->interpolation = interpolation;
    existing->cp1_x = cp1_x;
    existing->cp1_y = cp1_y;
    existing->cp2_x = cp2_x;
    existing->cp2_y = cp2_y;
    return;
   }
   KeyFrameT<T> keyframe{frame, value};
   keyframe.interpolation = interpolation;
   keyframe.cp1_x = cp1_x;
   keyframe.cp1_y = cp1_y;
   keyframe.cp2_x = cp2_x;
   keyframe.cp2_y = cp2_y;
   keyframes_.append(std::move(keyframe));
   normalizeKeyFrames();
  }

  // ============================================
  // �L�[�t���[���Ǘ��@�\
  // ============================================
  
  // �w��t���[���ɃL�[�t���[�������݂��邩
  bool hasKeyFrameAt(const FramePosition& frame) const {
   std::shared_lock lock(mutex_);
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   return it != keyframes_.end();
  }
  
  // �w��t���[���̃L�[�t���[�����폜
  void removeKeyFrameAt(const FramePosition& frame) {
   std::unique_lock lock(mutex_);
   keyframes_.removeIf(
    [&frame](const auto& kf) { return kf.frame == frame; });
  }

  bool moveKeyFrame(const FramePosition& from, const FramePosition& to) {
   std::unique_lock lock(mutex_);
   if (from == to) {
    return std::find_if(keyframes_.begin(), keyframes_.end(),
      [&from](const auto& kf) { return kf.frame == from; }) != keyframes_.end();
   }

   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&from](const auto& kf) { return kf.frame == from; });
   if (it == keyframes_.end()) return false;

   // Removing the destination can invalidate `it` (especially when the
   // destination is after the source), so copy the source before erasing.
   KeyFrameT<T> moved = *it;
   const auto sourceIndex = static_cast<std::size_t>(std::distance(keyframes_.begin(), it));
   keyframes_.takeAt(sourceIndex);
   keyframes_.removeIf(
    [&to](const auto& kf) { return kf.frame == to; });
   moved.frame = to;
   keyframes_.append(std::move(moved));
   normalizeKeyFrames();
   return true;
  }

  bool setKeyFrameInterpolationAt(const FramePosition& frame, InterpolationType interpolation) {
   std::unique_lock lock(mutex_);
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (it == keyframes_.end()) return false;
   it->interpolation = interpolation;
   return true;
  }

  bool setKeyFrameValueAt(const FramePosition& frame, const T& value) {
   std::unique_lock lock(mutex_);
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (it == keyframes_.end()) return false;
   it->value = value;
   return true;
  }

  bool setKeyFrameBezierAt(const FramePosition& frame,
                           float cp1_x, float cp1_y, float cp2_x, float cp2_y) {
   std::unique_lock lock(mutex_);
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (it == keyframes_.end()) return false;
   it->cp1_x = cp1_x;
   it->cp1_y = cp1_y;
   it->cp2_x = cp2_x;
   it->cp2_y = cp2_y;
   return true;
  }

  InterpolationType getKeyFrameInterpolationAt(const FramePosition& frame) const {
   std::shared_lock lock(mutex_);
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (it == keyframes_.end()) return static_cast<InterpolationType>(0);
   return it->interpolation;
  }
  
  // ���ׂẴL�[�t���[�����N���A
  void clearKeyFrames() {
   std::unique_lock lock(mutex_);
   keyframes_.clear();
  }
  
  // �L�[�t���[�������擾
  size_t getKeyFrameCount() const {
   std::shared_lock lock(mutex_);
   return keyframes_.size();
  }
  
  // ���ׂẴL�[�t���[�����擾�i�ǂݎ���p�j
  std::vector<KeyFrameT<T>> getKeyFrames() const {
   std::shared_lock lock(mutex_);
   return keyframes_.toStdVector();
  }

  std::vector<FramePosition> getKeyFrameFrames() const {
   std::shared_lock lock(mutex_);
   std::vector<FramePosition> frames;
   frames.reserve(keyframes_.size());
   for (const auto& kf : keyframes_) {
    frames.push_back(kf.frame);
   }
   return frames;
  }

  
 };

export enum class AnimationLayerBlendMode : std::uint8_t {
  Additive,
  Override
};

export struct AnimationLayerState {
  AnimationLayerBlendMode blendMode = AnimationLayerBlendMode::Additive;
  float weight = 1.0f;
  bool muted = false;
  bool solo = false;
};

// Non-destructive property stack used by animation-layer clients. The base
// value remains untouched; additive layers contribute a weighted delta from
// the base and override layers blend toward their evaluated value.
export template<typename T>
class AnimationLayerStackT {
public:
  struct Layer {
    AnimationLayerState state;
    AnimatableValueT<T> values;
  };

  explicit AnimationLayerStackT(const T& base = T{}) : base_(base) {}

  void setBase(const T& value) { base_.setCurrent(value); }
  T base(const FramePosition& frame) const { return base_.at(frame); }

  std::size_t layerCount() const { return layers_.size(); }
  Layer& layer(std::size_t index) { return layers_.at(index); }
  const Layer& layer(std::size_t index) const { return layers_.at(index); }
  std::size_t addLayer(const AnimationLayerState& state = {}) {
    layers_.push_back(Layer{state, AnimatableValueT<T>{}});
    return layers_.size() - 1;
  }
  void removeLayer(std::size_t index) {
    if (index < layers_.size()) layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(index));
  }
  void clear() { layers_.clear(); }

  T evaluate(const FramePosition& frame) const {
    return evaluateWithBase(frame, base_.at(frame));
  }

  T evaluateWithBase(const FramePosition& frame, const T& baseValue) const {
    const bool hasSolo = std::any_of(layers_.begin(), layers_.end(),
                                     [](const Layer& layer) { return layer.state.solo && !layer.state.muted; });
    T result = baseValue;
    for (const auto& layer : layers_) {
      if (layer.state.muted || (hasSolo && !layer.state.solo)) continue;
      const float weight = std::clamp(layer.state.weight, 0.0f, 1.0f);
      if (weight <= 0.0f) continue;
      const T value = layer.values.at(frame);
      if (layer.state.blendMode == AnimationLayerBlendMode::Additive) {
        result = result + (value - baseValue) * weight;
      } else {
        result = result + (value - result) * weight;
      }
    }
    return result;
  }

  // NOTE: layer persistence is float-only by design (all production stacks
  // are AnimationLayerStackT<float>). Non-float T round-trips as an empty
  // object; generalizing needs per-T QVariant conversion traits.
  QJsonObject toJson() const {
    QJsonObject object;
    if constexpr (std::is_same_v<T, float>) {
      object[QStringLiteral("base")] = static_cast<double>(base_.current());
      QJsonArray layers;
      for (const auto& layer : layers_) {
        QJsonObject layerObject;
        layerObject[QStringLiteral("mode")] =
            layer.state.blendMode == AnimationLayerBlendMode::Override ? QStringLiteral("override")
                                                                         : QStringLiteral("additive");
        layerObject[QStringLiteral("weight")] = static_cast<double>(layer.state.weight);
        layerObject[QStringLiteral("muted")] = layer.state.muted;
        layerObject[QStringLiteral("solo")] = layer.state.solo;
        QJsonArray keyframes;
        for (const auto& keyframe : layer.values.getKeyFrames()) {
          QJsonObject keyframeObject;
          keyframeObject[QStringLiteral("frame")] =
              static_cast<qint64>(keyframe.frame.framePosition());
          keyframeObject[QStringLiteral("value")] = static_cast<double>(keyframe.value);
          keyframeObject[QStringLiteral("interpolation")] =
              static_cast<int>(keyframe.interpolation);
          keyframeObject[QStringLiteral("cp1_x")] = static_cast<double>(keyframe.cp1_x);
          keyframeObject[QStringLiteral("cp1_y")] = static_cast<double>(keyframe.cp1_y);
          keyframeObject[QStringLiteral("cp2_x")] = static_cast<double>(keyframe.cp2_x);
          keyframeObject[QStringLiteral("cp2_y")] = static_cast<double>(keyframe.cp2_y);
          keyframes.append(keyframeObject);
        }
        layerObject[QStringLiteral("keyframes")] = keyframes;
        layers.append(layerObject);
      }
      object[QStringLiteral("layers")] = layers;
    }
    return object;
  }

  void fromJson(const QJsonObject& object) {
    if constexpr (std::is_same_v<T, float>) {
      base_.setCurrent(static_cast<float>(object.value(QStringLiteral("base")).toDouble()));
      layers_.clear();
      const QJsonArray layers = object.value(QStringLiteral("layers")).toArray();
      for (const auto& value : layers) {
        const QJsonObject layerObject = value.toObject();
        AnimationLayerState state;
        state.blendMode = layerObject.value(QStringLiteral("mode")).toString() == QStringLiteral("override")
                              ? AnimationLayerBlendMode::Override
                              : AnimationLayerBlendMode::Additive;
        state.weight = static_cast<float>(layerObject.value(QStringLiteral("weight")).toDouble(1.0));
        state.muted = layerObject.value(QStringLiteral("muted")).toBool(false);
        state.solo = layerObject.value(QStringLiteral("solo")).toBool(false);
        const std::size_t index = addLayer(state);
        for (const auto& keyframeValue : layerObject.value(QStringLiteral("keyframes")).toArray()) {
          const QJsonObject keyframe = keyframeValue.toObject();
          const FramePosition frame(
              keyframe.value(QStringLiteral("frame")).toInteger());
          layers_[index].values.addKeyFrame(
              frame, static_cast<float>(keyframe.value(QStringLiteral("value")).toDouble()),
              static_cast<InterpolationType>(
                  keyframe.value(QStringLiteral("interpolation")).toInt(0)),
              static_cast<float>(keyframe.value(QStringLiteral("cp1_x")).toDouble(0.42)),
              static_cast<float>(keyframe.value(QStringLiteral("cp1_y")).toDouble(0.0)),
              static_cast<float>(keyframe.value(QStringLiteral("cp2_x")).toDouble(0.58)),
              static_cast<float>(keyframe.value(QStringLiteral("cp2_y")).toDouble(1.0)));
        }
      }
    }
  }

 private:
  AnimatableValueT<T> base_;
  std::vector<Layer> layers_;
};

// =========================
// Reusable automation clips (Phase 2, Bitwig-inspired)
// =========================
// Ownership contract (see Phase 0 contract doc): a pattern is
// composition-owned shared curve data; instances are layer-owned placements
// referencing a pattern by id. All evaluation below is stateless and
// allocation-free; sorting/sanitizing happens on the cold path. Float
// channels only in Phase 2 (Transform/Opacity); Color remap is deferred.

export enum class AutomationClipLoopMode : std::uint8_t {
  Off,
  Loop,
  PingPong
};

export enum class AutomationClipTimePolicy : std::uint8_t {
  ParentFollow,
  FreeTime
};

export struct AutomationClipPoint {
  double time = 0.0;  // seconds, relative to pattern start, >= 0 after sanitize
  float value = 0.0f;
  InterpolationType interpolation = InterpolationType::Linear;
  // Reserved curvature bias in [-1, 1] for future hermite shaping. Persisted
  // and round-tripped, but not yet applied by evaluation.
  float curvature = 0.0f;
  float cp1_x = 0.42f, cp1_y = 0.0f;
  float cp2_x = 0.58f, cp2_y = 1.0f;
};

export constexpr std::size_t kMaxAutomationClipPoints = 4096;

// Phase 2 evaluation coverage: only these layer-relative paths are read by
// the layer evaluator. Instances on other paths persist (forward compatible)
// but stay inert until later phases wire them.
export inline bool isAutomationClipEvaluatedPath(std::string_view path) noexcept {
  return path == "transform.position.x" || path == "transform.position.y" ||
         path == "transform.rotation" || path == "transform.scale.x" ||
         path == "transform.scale.y" || path == "layer.opacity";
}

export struct AutomationClipPattern {
  std::uint32_t id{0};  // 0 = invalid/reserved, consistent with source ids
  std::string name;
  std::uint32_t seed{2463534242u};
  std::vector<AutomationClipPoint> points;  // sorted by time, cold-path maintained

  double duration() const {
    if (points.empty()) {
      return 0.0;
    }
    const double last = points.back().time;
    return std::isfinite(last) ? std::max(0.0, last) : 0.0;
  }
};

export struct AutomationClipInstance {
  std::uint32_t patternId{0};
  std::string targetPath;  // layer-relative property path, e.g. "transform.position.x"
  double offsetSeconds = 0.0;
  double stretch = 1.0;  // sanitized to > 0 at evaluation
  AutomationClipLoopMode loop = AutomationClipLoopMode::Off;
  AutomationClipTimePolicy timePolicy = AutomationClipTimePolicy::ParentFollow;
  float weight = 1.0f;  // 0..1 mix toward the clip value
  bool enabled = true;
};

// Maps a pattern-local time into [0, duration] per loop policy. No allocation.
export inline double mapAutomationClipLoop(double t, double duration,
                                           AutomationClipLoopMode loop) noexcept {
  if (!(duration > 0.0)) {
    return 0.0;
  }
  if (!std::isfinite(t)) {
    t = 0.0;
  }
  switch (loop) {
    case AutomationClipLoopMode::Loop: {
      double wrapped = std::fmod(t, duration);
      if (wrapped < 0.0) {
        wrapped += duration;
      }
      return wrapped;
    }
    case AutomationClipLoopMode::PingPong: {
      const double period = duration * 2.0;
      double wrapped = std::fmod(t, period);
      if (wrapped < 0.0) {
        wrapped += period;
      }
      return wrapped > duration ? period - wrapped : wrapped;
    }
    case AutomationClipLoopMode::Off:
    default:
      return std::clamp(t, 0.0, duration);
  }
}

// Evaluates pattern points at a loop-mapped local time. No allocation.
export inline float evaluateAutomationClipPattern(
    const AutomationClipPattern& pattern, double localSeconds) {
  const auto& points = pattern.points;
  if (points.empty()) {
    return 0.0f;
  }
  if (points.size() == 1) {
    const float single = points.front().value;
    return std::isfinite(single) ? single : 0.0f;
  }
  double t = std::isfinite(localSeconds) ? localSeconds : 0.0;
  if (t <= points.front().time) {
    return points.front().value;
  }
  if (t >= points.back().time) {
    return points.back().value;
  }
  const auto it = std::lower_bound(
      points.begin(), points.end(), t,
      [](const AutomationClipPoint& point, double time) { return point.time < time; });
  const auto& next = *it;
  const auto& prev = *std::prev(it);
  const double span = next.time - prev.time;
  float alpha = span > 0.0 ? static_cast<float>((t - prev.time) / span) : 0.0f;
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  const float a = std::isfinite(prev.value) ? prev.value : 0.0f;
  const float b = std::isfinite(next.value) ? next.value : 0.0f;
  if (prev.interpolation == InterpolationType::Bezier) {
    const float cp1x = std::isfinite(prev.cp1_x) ? prev.cp1_x : 0.42f;
    const float cp1y = std::isfinite(prev.cp1_y) ? prev.cp1_y : 0.0f;
    const float cp2x = std::isfinite(prev.cp2_x) ? prev.cp2_x : 0.58f;
    const float cp2y = std::isfinite(prev.cp2_y) ? prev.cp2_y : 1.0f;
    return bezierInterpolate(a, b, alpha, cp1x, cp1y, cp2x, cp2y);
  }
  return interpolate(a, b, alpha, prev.interpolation);
}

// Applies one instance over a base value. weight == 0 (or disabled/mismatch)
// returns baseValue exactly. No allocation.
export inline float applyAutomationClipInstance(
    float baseValue, const AutomationClipPattern& pattern,
    const AutomationClipInstance& instance, double parentSeconds) {
  if (!instance.enabled || instance.patternId == 0 ||
      pattern.id != instance.patternId) {
    return baseValue;
  }
  if (!std::isfinite(baseValue) || !std::isfinite(parentSeconds)) {
    return baseValue;
  }
  const double offset =
      std::isfinite(instance.offsetSeconds) ? instance.offsetSeconds : 0.0;
  const double stretch = (std::isfinite(instance.stretch) && instance.stretch > 0.0)
      ? instance.stretch : 1.0;
  const double mapped = mapAutomationClipLoop(
      (parentSeconds - offset) / stretch, pattern.duration(), instance.loop);
  const float clipValue = evaluateAutomationClipPattern(pattern, mapped);
  const float weight = std::isfinite(instance.weight)
      ? std::clamp(instance.weight, 0.0f, 1.0f) : 1.0f;
  if (weight <= 0.0f) {
    return baseValue;
  }
  const float result = baseValue + (clipValue - baseValue) * weight;
  return std::isfinite(result) ? result : baseValue;
}

// Cold-path sanitizer: sorts by time, clamps ranges, caps size.
// Returns false when no usable points remain.
export inline bool sanitizeAutomationClipPattern(AutomationClipPattern& pattern) {
  if (pattern.points.size() > kMaxAutomationClipPoints) {
    pattern.points.resize(kMaxAutomationClipPoints);
  }
  for (auto& point : pattern.points) {
    if (!std::isfinite(point.time) || point.time < 0.0) {
      point.time = 0.0;
    }
    if (!std::isfinite(point.value)) {
      point.value = 0.0f;
    }
    if (!std::isfinite(point.curvature)) {
      point.curvature = 0.0f;
    }
    point.curvature = std::clamp(point.curvature, -1.0f, 1.0f);
    if (!std::isfinite(point.cp1_x)) point.cp1_x = 0.42f;
    if (!std::isfinite(point.cp1_y)) point.cp1_y = 0.0f;
    if (!std::isfinite(point.cp2_x)) point.cp2_x = 0.58f;
    if (!std::isfinite(point.cp2_y)) point.cp2_y = 1.0f;
  }
  std::stable_sort(pattern.points.begin(), pattern.points.end(),
      [](const AutomationClipPoint& a, const AutomationClipPoint& b) {
        return a.time < b.time;
      });
  return !pattern.points.empty();
}

export inline bool automationClipInstancesEqual(
    const std::vector<AutomationClipInstance>& a,
    const std::vector<AutomationClipInstance>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto& x = a[i];
    const auto& y = b[i];
    if (x.patternId != y.patternId || x.targetPath != y.targetPath ||
        x.offsetSeconds != y.offsetSeconds || x.stretch != y.stretch ||
        x.loop != y.loop || x.timePolicy != y.timePolicy ||
        x.weight != y.weight || x.enabled != y.enabled) {
      return false;
    }
  }
  return true;
}

// Qt-boundary JSON converters (cold path only: project save/load, undo).
export inline QJsonObject automationClipPatternToJson(
    const AutomationClipPattern& pattern) {
  QJsonObject object;
  object[QStringLiteral("id")] = static_cast<double>(pattern.id);
  object[QStringLiteral("name")] = QString::fromStdString(pattern.name);
  object[QStringLiteral("seed")] = static_cast<double>(pattern.seed);
  QJsonArray points;
  for (const auto& point : pattern.points) {
    QJsonObject entry;
    entry[QStringLiteral("t")] = point.time;
    entry[QStringLiteral("v")] = static_cast<double>(point.value);
    entry[QStringLiteral("interp")] = static_cast<int>(point.interpolation);
    entry[QStringLiteral("curv")] = static_cast<double>(point.curvature);
    entry[QStringLiteral("cp1_x")] = static_cast<double>(point.cp1_x);
    entry[QStringLiteral("cp1_y")] = static_cast<double>(point.cp1_y);
    entry[QStringLiteral("cp2_x")] = static_cast<double>(point.cp2_x);
    entry[QStringLiteral("cp2_y")] = static_cast<double>(point.cp2_y);
    points.append(entry);
  }
  object[QStringLiteral("points")] = points;
  return object;
}

export inline bool automationClipPatternFromJson(
    const QJsonObject& object, AutomationClipPattern& pattern) {
  const double rawId = object.value(QStringLiteral("id")).toDouble(0.0);
  if (!std::isfinite(rawId) || rawId < 1.0 ||
      rawId > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
      std::floor(rawId) != rawId) {
    return false;
  }
  const QJsonValue pointsValue = object.value(QStringLiteral("points"));
  if (!pointsValue.isArray()) {
    return false;
  }
  AutomationClipPattern result;
  result.id = static_cast<std::uint32_t>(rawId);
  result.name = object.value(QStringLiteral("name")).toString().toStdString();
  result.seed = static_cast<std::uint32_t>(
      object.value(QStringLiteral("seed")).toVariant().toUInt());
  for (const auto& value : pointsValue.toArray()) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject entry = value.toObject();
    AutomationClipPoint point;
    point.time = entry.value(QStringLiteral("t")).toDouble(0.0);
    point.value = static_cast<float>(entry.value(QStringLiteral("v")).toDouble(0.0));
    point.interpolation = static_cast<InterpolationType>(
        std::clamp(entry.value(QStringLiteral("interp")).toInt(0), 0, 64));
    point.curvature = static_cast<float>(entry.value(QStringLiteral("curv")).toDouble(0.0));
    point.cp1_x = static_cast<float>(entry.value(QStringLiteral("cp1_x")).toDouble(0.42));
    point.cp1_y = static_cast<float>(entry.value(QStringLiteral("cp1_y")).toDouble(0.0));
    point.cp2_x = static_cast<float>(entry.value(QStringLiteral("cp2_x")).toDouble(0.58));
    point.cp2_y = static_cast<float>(entry.value(QStringLiteral("cp2_y")).toDouble(1.0));
    result.points.push_back(point);
    if (result.points.size() >= kMaxAutomationClipPoints) {
      break;
    }
  }
  if (!sanitizeAutomationClipPattern(result)) {
    return false;
  }
  pattern = std::move(result);
  return true;
}

export inline QJsonObject automationClipInstanceToJson(
    const AutomationClipInstance& instance) {
  QJsonObject object;
  object[QStringLiteral("patternId")] = static_cast<double>(instance.patternId);
  object[QStringLiteral("targetPath")] = QString::fromStdString(instance.targetPath);
  object[QStringLiteral("offset")] = instance.offsetSeconds;
  object[QStringLiteral("stretch")] = instance.stretch;
  object[QStringLiteral("loop")] = static_cast<int>(instance.loop);
  object[QStringLiteral("timePolicy")] = static_cast<int>(instance.timePolicy);
  object[QStringLiteral("weight")] = static_cast<double>(instance.weight);
  object[QStringLiteral("enabled")] = instance.enabled;
  return object;
}

export inline bool automationClipInstanceFromJson(
    const QJsonObject& object, AutomationClipInstance& instance) {
  const double rawId = object.value(QStringLiteral("patternId")).toDouble(0.0);
  if (!std::isfinite(rawId) || rawId < 1.0 ||
      rawId > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
      std::floor(rawId) != rawId) {
    return false;
  }
  const QString targetPath =
      object.value(QStringLiteral("targetPath")).toString().trimmed();
  if (targetPath.isEmpty()) {
    return false;
  }
  AutomationClipInstance result;
  result.patternId = static_cast<std::uint32_t>(rawId);
  result.targetPath = targetPath.toStdString();
  result.offsetSeconds = object.value(QStringLiteral("offset")).toDouble(0.0);
  result.stretch = object.value(QStringLiteral("stretch")).toDouble(1.0);
  result.loop = static_cast<AutomationClipLoopMode>(
      std::clamp(object.value(QStringLiteral("loop")).toInt(0), 0, 2));
  result.timePolicy = static_cast<AutomationClipTimePolicy>(
      std::clamp(object.value(QStringLiteral("timePolicy")).toInt(0), 0, 1));
  result.weight = static_cast<float>(object.value(QStringLiteral("weight")).toDouble(1.0));
  result.enabled = object.value(QStringLiteral("enabled")).toBool(true);
  instance = std::move(result);
  return true;
}


};
