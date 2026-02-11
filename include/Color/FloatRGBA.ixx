module;

export module FloatRGBA;

import std;

export namespace ArtifactCore {

 class FloatColor;

 //class FloatRGBAPrivate;

 class FloatRGBA {
 private:
  float r_, g_, b_, a_;

 public:
  // コンストラクタ
  constexpr FloatRGBA() : r_(0), g_(0), b_(0), a_(0) {}
  constexpr FloatRGBA(float r, float g, float b, float a = 1.0f)
   : r_(r), g_(g), b_(b), a_(a) {
  }

  // コピー・ムーブコンストラクタ
  FloatRGBA(const FloatRGBA&) = default;
  FloatRGBA(FloatRGBA&&) = default;

  // アクセッサ
  float r() const { return r_; }
  float g() const { return g_; }
  float b() const { return b_; }
  float a() const { return a_; }

  void setRed(float r) { r_ = r; }
  void setGreen(float g) { g_ = g; }
  void setBlue(float b) { b_ = b; }
  void setAlpha(float a) { a_ = a; }  // ← fix: Apha → Alpha

  void setRGBA(float r, float g, float b = 0.0f, float a = 0.0f) {
   r_ = r; g_ = g; b_ = b; a_ = a;
  }

  // 添字演算子（要範囲チェック）
  float& operator[](int index) {
   if (index < 0 || index > 3) throw std::out_of_range("FloatRGBA index");
   return (&r_)[index];
  }

  const float& operator[](int index) const {
   if (index < 0 || index > 3) throw std::out_of_range("FloatRGBA index");
   return (&r_)[index];
  }

  // 算術演算子
  FloatRGBA operator+(const FloatRGBA& rhs) const {
   return FloatRGBA(r_ + rhs.r_, g_ + rhs.g_, b_ + rhs.b_, a_ + rhs.a_);
  }

  FloatRGBA operator*(float scalar) const {
   return FloatRGBA(r_ * scalar, g_ * scalar, b_ * scalar, a_ * scalar);
  }

  // 代入演算子
  FloatRGBA& operator=(const FloatRGBA&) = default;
  FloatRGBA& operator=(FloatRGBA&&) = default;

  // 変換演算子
  operator FloatColor() const; // 実装は別途

  // ユーティリティ
  void setFromFloatColor(const FloatColor& color);
  void setFromRandom(); // ランダム値で初期化

  // スワップ
  void swap(FloatRGBA& other) noexcept {
   std::swap(r_, other.r_);
   std::swap(g_, other.g_);
   std::swap(b_, other.b_);
   std::swap(a_, other.a_);
  }
 };



};