module;
export module Animation.Animatable;

import std;

import Core.KeyFrame;

export namespace ArtifactCore {

 template<typename T, typename TimeType = FramePosition>
 class Animatable {
 public:
  struct KeyFrame {
   TimeType time;
   T value;
  };

 private:
  std::vector<KeyFrame> keyframes_;

 public:
  void addKeyFrame(const TimeType& time, const T& value) {
   keyframes_.push_back({ time, value });
   std::sort(keyframes_.begin(), keyframes_.end(),
	[](const KeyFrame& a, const KeyFrame& b) { return a.time < b.time; });
  }

  T valueAt(const TimeType& t) const {
   if (keyframes_.empty()) return T{};
   if (t <= keyframes_.front().time) return keyframes_.front().value;
   if (t >= keyframes_.back().time)  return keyframes_.back().value;

   // 補間
   for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
	const auto& k0 = keyframes_[i];
	const auto& k1 = keyframes_[i + 1];
	if (t >= k0.time && t <= k1.time) {
	 float ratio = float((t - k0.time) / (k1.time - k0.time));  // FramePosition なら operator- / operator/ 定義しておく
	 return k0.value + (k1.value - k0.value) * ratio;
	}
   }

   return keyframes_.back().value;
  }

  const std::vector<KeyFrame>& keyframes() const { return keyframes_; }
 };

};