module;

export module Animation.Value;

import std;
import Core.KeyFrame;
import Frame.Position;

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

  // キーフレーム追加
  void addKeyFrame(const FramePosition& frame, const T& value) {
   keyframes_.push_back({ frame, value });
   // 必要なら frame でソート
   std::sort(keyframes_.begin(), keyframes_.end(),
	[](auto& a, auto& b) { return a.frame < b.frame; });
  }


  
 };


};