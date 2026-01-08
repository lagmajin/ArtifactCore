module;

export module Animation.Value;

import std;
import Core.KeyFrame;
import Frame.Position;
import Math.Interpolate;


export namespace ArtifactCore {

 template<typename T>
 struct KeyFrameT {
  FramePosition frame;
  T value;
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


  
 };


};