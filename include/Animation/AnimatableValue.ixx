module;

export module Animation.Value;

import std;
import Core.KeyFrame;
import Frame.Position;
import Math.Interpolate;


export namespace ArtifactCore {

// ヘルパー関数：線形補間の係数計算
inline float calculateT(const FramePosition& start, const FramePosition& end, const FramePosition& current) {
 std::int64_t startFrame = start.framePosition();
 std::int64_t endFrame = end.framePosition();
 std::int64_t currentFrame = current.framePosition();
  
 if (endFrame <= startFrame) return 0.0f;
  
 float range = static_cast<float>(endFrame - startFrame);
 float offset = static_cast<float>(currentFrame - startFrame);
 return offset / range;
}

// ヘルパー関数：線形補間（lerp）
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

 public:
  AnimatableValueT() = default;

  // 現在値（キーフレームを作らない）
  void setCurrent(const T& v) {
   currentValue_ = v;
  }

  const T& current() const {
   return currentValue_;
  }

  T at(const FramePosition& frame) const {
   if (keyframes_.empty()) return currentValue_;
   if (keyframes_.size() == 1) return keyframes_[0].value;

   // 1. 指定フレームが全キーフレームより前か後かチェック
   if (frame <= keyframes_.front().frame) return keyframes_.front().value;
   if (frame >= keyframes_.back().frame) return keyframes_.back().value;

   // 2. 二分探索で「今どのキーフレーム間にいるか」を探す (std::lower_bound)
   auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), frame,
	[](const auto& kf, const auto& f) { return kf.frame < f; });

   // 3. 前後のキーフレームを取得して線形補間（Lerp）
   auto next = it;
   auto prev = std::prev(it);

   float t = calculateT(prev->frame, next->frame, frame); // 0.0 ~ 1.0 の割合
   return mix(prev->value, next->value, t); // 線形補間
  }
 	
  // キーフレーム追加
  void addKeyFrame(const FramePosition& frame, const T& value) {
   keyframes_.push_back({ frame, value });
   // 必要なら frame でソート
   std::sort(keyframes_.begin(), keyframes_.end(),
	[](auto& a, auto& b) { return a.frame < b.frame; });
  }

  // ============================================
  // キーフレーム管理機能
  // ============================================
  
  // 指定フレームにキーフレームが存在するか
  bool hasKeyFrameAt(const FramePosition& frame) const {
   auto it = std::find_if(keyframes_.begin(), keyframes_.end(),
    [&frame](const auto& kf) { return kf.frame == frame; });
   return it != keyframes_.end();
  }
  
  // 指定フレームのキーフレームを削除
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
  
  // すべてのキーフレームをクリア
  void clearKeyFrames() {
   keyframes_.clear();
  }
  
  // キーフレーム数を取得
  size_t getKeyFrameCount() const {
   return keyframes_.size();
  }
  
  // すべてのキーフレームを取得（読み取り専用）
  const std::vector<KeyFrameT<T>>& getKeyFrames() const {
   return keyframes_;
  }

  
 };


};