module;

export module Animation.Value;

import std;
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


 // =========================
 // AnimatableValueT<T>
 // =========================
 template<typename T>
 class AnimatableValueT {
 private:
  std::vector<KeyFrameT<T>> keyframes_;
  T currentValue_{};
  mutable size_t lastCachedIndex_ = 0; // Temporal Coherence用のキャッシュ

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

   // 2. Temporal Coherence Optimization (O(1) path for sequential playback)
   size_t n = keyframes_.size();
   if (lastCachedIndex_ < n - 1) {
       const auto& kfPrev = keyframes_[lastCachedIndex_];
       const auto& kfNext = keyframes_[lastCachedIndex_ + 1];

       if (frame >= kfPrev.frame && frame < kfNext.frame) {
           float t = calculateT(kfPrev.frame, kfNext.frame, frame);
           return mix(kfPrev.value, kfNext.value, t);
       }
       // 連続再生で次の区間に進んだ場合
       if (lastCachedIndex_ + 2 < n && frame >= kfNext.frame && frame < keyframes_[lastCachedIndex_ + 2].frame) {
           lastCachedIndex_++;
           const auto& kfNext2 = keyframes_[lastCachedIndex_ + 1];
           float t = calculateT(kfNext.frame, kfNext2.frame, frame);
           return mix(kfNext.value, kfNext2.value, t); // Fixed: should be value, not frame
       }
   }

   // 3. Fallback: Binary Search (O(log N))
   auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), frame,
 [](const auto& kf, const auto& f) { return kf.frame < f; });

   auto next = it;
   auto prev = std::prev(it);

   // キャッシュの更新
   lastCachedIndex_ = std::distance(keyframes_.begin(), prev);

   float t = calculateT(prev->frame, next->frame, frame); // 0.0 ~ 1.0 ̊
   return mix(prev->value, next->value, t); // `
  }

  // L[t[ǉ
  void addKeyFrame(const FramePosition& frame, const T& value) {
   keyframes_.push_back({ frame, value });
   std::sort(keyframes_.begin(), keyframes_.end(),
 [](auto& a, auto& b) { return a.frame < b.frame; });
   lastCachedIndex_ = 0; // キャッシュ無効化
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
   return true;
  }

  bool setKeyFrameInterpolationAt(const FramePosition& frame, InterpolationType interpolation) {
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   if (it == keyframes_.end()) return false;
   it->interpolation = interpolation;
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
  }
  
  // �L�[�t���[�������擾
  size_t getKeyFrameCount() const {
   return keyframes_.size();
  }
  
  // ���ׂẴL�[�t���[�����擾�i�ǂݎ���p�j
  const std::vector<KeyFrameT<T>>& getKeyFrames() const {
   return keyframes_;
  }

  
 };


};