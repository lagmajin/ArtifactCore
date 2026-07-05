module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>
export module Animation.Value;

import Frame.Position;


export namespace ArtifactCore {

enum class InterpolationType : int;

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
  std::vector<KeyFrameT<T>> keyframes_;
  T currentValue_{};
  mutable size_t lastCachedIndex_ = 0; // Temporal Coherence用のキャッシュ

  void invalidateCache() {
   lastCachedIndex_ = 0;
  }

 public:
  AnimatableValueT() = default;

  // ݒliL[t[Ȃj
  void setCurrent(const T& v) {
   currentValue_ = v;
  }

  const T& current() const {
   return currentValue_;
  }

  T at(const FramePosition& frame) const {
   if (keyframes_.empty()) return currentValue_;
   if (keyframes_.size() == 1) return keyframes_[0].value;

   // 1. wt[SL[t[Oォ`FbN
   if (frame <= keyframes_.front().frame) return keyframes_.front().value;
   if (frame >= keyframes_.back().frame) return keyframes_.back().value;

   // 2. Temporal Coherence Optimization
   size_t n = keyframes_.size();
   if (lastCachedIndex_ < n - 1) {
       const auto& kfPrev = keyframes_[lastCachedIndex_];
       const auto& kfNext = keyframes_[lastCachedIndex_ + 1];

       if (frame >= kfPrev.frame && frame < kfNext.frame) {
           float t = calculateT(kfPrev.frame, kfNext.frame, frame);
           return interpolateValue(kfPrev.value, kfNext.value, t, kfPrev.interpolation);
       }
       if (lastCachedIndex_ + 2 < n && frame >= kfNext.frame && frame < keyframes_[lastCachedIndex_ + 2].frame) {
           lastCachedIndex_++;
           const auto& kfNext2 = keyframes_[lastCachedIndex_ + 1];
           float t = calculateT(kfNext.frame, kfNext2.frame, frame);
           return interpolateValue(kfNext.value, kfNext2.value, t, kfNext.interpolation);
       }
   }

   // 3. Fallback: Binary Search
   auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), frame,
 [](const auto& kf, const auto& f) { return kf.frame < f; });

   auto next = it;
   auto prev = std::prev(it);
   lastCachedIndex_ = std::distance(keyframes_.begin(), prev);

   float t = calculateT(prev->frame, next->frame, frame);
   return interpolateValue(prev->value, next->value, t, prev->interpolation);
  }

  // 物理ベースの評価 (Spring-Damper)
  float atSpring(const FramePosition& frame, float dt, SpringState& state) const {
      float target = static_cast<float>(at(frame));
      if (!state.initialized) {
          state.currentValue = target;
          state.velocity = 0.0f;
          state.initialized = true;
          return target;
      }

      // Semi-implicit Euler integration
      float force = -state.stiffness * (state.currentValue - target) - state.damping * state.velocity;
      state.velocity += (force / state.mass) * dt;
      state.currentValue += state.velocity * dt;

      return state.currentValue;
  }

  // L[t[ǉ
  void addKeyFrame(const FramePosition& frame, const T& value) {
   keyframes_.push_back({ frame, value });
   std::sort(keyframes_.begin(), keyframes_.end(),
 [](auto& a, auto& b) { return a.frame < b.frame; });
   invalidateCache();
  }

  // ============================================
  // �L�[�t���[���Ǘ��@�\
  // ============================================
  
  // �w��t���[���ɃL�[�t���[�������݂��邩
  bool hasKeyFrameAt(const FramePosition& frame) const {
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   return it != keyframes_.end();
  }
  
  // �w��t���[���̃L�[�t���[�����폜
  void removeKeyFrameAt(const FramePosition& frame) {
   auto it = std::remove_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   keyframes_.erase(it, keyframes_.end());
   invalidateCache();
  }

  bool moveKeyFrame(const FramePosition& from, const FramePosition& to) {
   if (from == to) return hasKeyFrameAt(from);

   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&from](const auto& kf) { return kf.frame == from; });
   if (it == keyframes_.end()) return false;

   removeKeyFrameAt(to);
   it->frame = to;
   std::sort(keyframes_.begin(), keyframes_.end(),
    [](auto& a, auto& b) { return a.frame < b.frame; });
   invalidateCache();
   return true;
  }

  bool setKeyFrameInterpolationAt(const FramePosition& frame, InterpolationType interpolation) {
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (it == keyframes_.end()) return false;
   it->interpolation = interpolation;
   return true;
  }

  bool setKeyFrameValueAt(const FramePosition& frame, const T& value) {
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (it == keyframes_.end()) return false;
   it->value = value;
   return true;
  }

  InterpolationType getKeyFrameInterpolationAt(const FramePosition& frame) const {
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (it == keyframes_.end()) return static_cast<InterpolationType>(0);
   return it->interpolation;
  }
  
  // ���ׂẴL�[�t���[�����N���A
  void clearKeyFrames() {
   keyframes_.clear();
   invalidateCache();
  }
  
  // �L�[�t���[�������擾
  size_t getKeyFrameCount() const {
   return keyframes_.size();
  }
  
  // ���ׂẴL�[�t���[�����擾�i�ǂݎ���p�j
  std::vector<KeyFrameT<T>> getKeyFrames() const {
   return keyframes_;
  }

  std::vector<FramePosition> getKeyFrameFrames() const {
   std::vector<FramePosition> frames;
   frames.reserve(keyframes_.size());
   for (const auto& kf : keyframes_) {
    frames.push_back(kf.frame);
   }
   return frames;
  }

  
 };


};
