module;

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>
export module Animation.Value;

import Container.NamedVector;
import Core.KeyFrame;
import Frame.Position;
import Math.Interpolate;


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
 InterpolationType interpolation = InterpolationType::Linear;
};

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
  NamedVector<KeyFrameT<T>> keyframes_{makeNamedVector<KeyFrameT<T>>(ContainerName{"AnimatableKeyframes"})};
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
           return interpolate(kfPrev.value, kfNext.value, t, kfPrev.interpolation);
       }
       if (lastCachedIndex_ + 2 < n && frame >= kfNext.frame && frame < keyframes_[lastCachedIndex_ + 2].frame) {
           lastCachedIndex_++;
           const auto& kfNext2 = keyframes_[lastCachedIndex_ + 1];
           float t = calculateT(kfNext.frame, kfNext2.frame, frame);
           return interpolate(kfNext.value, kfNext2.value, t, kfNext.interpolation);
       }
   }

   // 3. Fallback: Binary Search
   auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), frame,
 [](const auto& kf, const auto& f) { return kf.frame < f; });

   auto next = it;
   auto prev = std::prev(it);
   lastCachedIndex_ = std::distance(keyframes_.begin(), prev);

   float t = calculateT(prev->frame, next->frame, frame);
   return interpolate(prev->value, next->value, t, prev->interpolation);
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
   keyframes_.add({ frame, value });
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
   if (it == keyframes_.end()) return InterpolationType::Linear;
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
   return keyframes_.toStdVector();
  }

  std::vector<FramePosition> getKeyFrameFrames() const {
   NamedVector<FramePosition> frames{makeNamedVector<FramePosition>(ContainerName{"AnimatableKeyframeFrames"})};
   frames.reserve(keyframes_.size());
   for (const auto& kf : keyframes_) {
    frames.add(kf.frame);
   }
   return frames.toStdVector();
  }

  
 };


};
