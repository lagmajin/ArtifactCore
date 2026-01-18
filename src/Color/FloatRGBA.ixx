module;

export module FloatRGBA;

import std;

class FloatColor;

export namespace ArtifactCore {

// class FloatColor;

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

  // アクセッサ
  float r() const;
  float g() const;
  float b() const;
  float a() const;

  void setRed(float r);
  void setGreen(float g);
  void setBlue(float b);
  void setAlpha(float a);  // ← fix: Apha → Alpha

  void setRGBA(float r, float g, float b = 0.0f, float a = 0.0f);

  // 添字演算子（要範囲チェック）
  float& operator[](int index) {
   if (index < 0 || index > 3) throw std::out_of_range("FloatRGBA index");
   return (&r_)[index];
  }

  const float& operator[](int index) const;

  // 算術演算子
  FloatRGBA operator+(const FloatRGBA& rhs) const;

  FloatRGBA operator*(float scalar) const;

  // 代入演算子
  FloatRGBA& operator=(const FloatRGBA&) = default;
  FloatRGBA& operator=(FloatRGBA&&) = default;

  // 変換演算子
  operator FloatColor() const; // 実装は別途

  // ユーティリティ
  void setFromFloatColor(const FloatColor& color);
  void setFromRandom(); // ランダム値で初期化

  // スワップ
  void swap(FloatRGBA& other) noexcept;
 };



};