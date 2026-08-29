module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
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
};

inline float interpolationAlpha(float alpha, InterpolationType type) {
 switch (static_cast<int>(type)) {
 case 1:
  return alpha < 1.0f ? 0.0f : 1.0f;
 case 3:
  return alpha * alpha;
 case 4: {
  const float u = 1.0f - alpha;
  return 1.0f - (u * u);
 }
 case 5:
  if (alpha < 0.5f) {
   return 2.0f * alpha * alpha;
  }
  return 1.0f - std::pow(-2.0f * alpha + 2.0f, 2.0f) * 0.5f;
 case 12:
  return std::sin((alpha * 3.14159265f) * 0.5f);
 case 8:
  return 1.0f - std::pow(1.0f - alpha, 3.0f);
 case 16:
  if (alpha < (1.0f / 2.75f)) {
   return 7.5625f * alpha * alpha;
  } else if (alpha < (2.0f / 2.75f)) {
   alpha -= (1.5f / 2.75f);
   return 7.5625f * alpha * alpha + 0.75f;
  } else if (alpha < (2.5f / 2.75f)) {
   alpha -= (2.25f / 2.75f);
   return 7.5625f * alpha * alpha + 0.9375f;
  }
  alpha -= (2.625f / 2.75f);
  return 7.5625f * alpha * alpha + 0.984375f;
 case 19:
  if (alpha <= 0.0f) return 0.0f;
  if (alpha >= 1.0f) return 1.0f;
  return std::pow(2.0f, -10.0f * alpha) *
             std::sin((alpha - 0.075f) * (2.0f * 3.14159265f) / 0.3f) +
         1.0f;
 case 22: {
  const float s = 1.70158f;
  alpha -= 1.0f;
  return alpha * alpha * ((s + 1.0f) * alpha + s) + 1.0f;
 }
 default:
  return alpha;
 }
}

template<typename T>
inline T interpolateValue(const T& start, const T& end, float alpha, InterpolationType type) {
 if (static_cast<int>(type) == 1) {
  return alpha < 1.0f ? start : end;
 }
 const float eased = interpolationAlpha(alpha, type);
 return start + (end - start) * eased;
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
   return interpolateValue(prev->value, next->value, t, prev->interpolation);
  }

  // 物理ベースの評価 (Spring-Damper)
  float atSpring(const FramePosition& frame, float dt, SpringState& state) const {
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
   // Replacing in place also preserves the existing interpolation mode.
   auto existing = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (existing != keyframes_.end()) {
    existing->value = value;
    return;
   }
   keyframes_.append({ frame, value });
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
              frame, static_cast<float>(keyframe.value(QStringLiteral("value")).toDouble()));
          layers_[index].values.setKeyFrameInterpolationAt(
              frame, static_cast<InterpolationType>(
                         keyframe.value(QStringLiteral("interpolation")).toInt(0)));
        }
      }
    }
  }

private:
  AnimatableValueT<T> base_;
  std::vector<Layer> layers_;
};


};
