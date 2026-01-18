module;

module FloatRGBA;



namespace ArtifactCore {





 FloatRGBA FloatRGBA::operator*(float scalar) const
 {
  return FloatRGBA(r_ * scalar, g_ * scalar, b_ * scalar, a_ * scalar);
 }

 void FloatRGBA::swap(FloatRGBA& other) noexcept
 {
  std::swap(r_, other.r_);
  std::swap(g_, other.g_);
  std::swap(b_, other.b_);
  std::swap(a_, other.a_);
 }

 FloatRGBA FloatRGBA::operator+(const FloatRGBA& rhs) const
 {
  return FloatRGBA(r_ + rhs.r_, g_ + rhs.g_, b_ + rhs.b_, a_ + rhs.a_);
 }

 const float& FloatRGBA::operator[](int index) const
 {
  if (index < 0 || index > 3) throw std::out_of_range("FloatRGBA index");
  return (&r_)[index];
 }

 void FloatRGBA::setRGBA(float r, float g, float b /*= 0.0f*/, float a /*= 0.0f*/)
 {
  r_ = r; g_ = g; b_ = b; a_ = a;
 }

 float FloatRGBA::r() const
 {
  return r_;
 }

 float FloatRGBA::g() const
 {
  return g_;
 }

 float FloatRGBA::b() const
 {
  return b_;
 }

 float FloatRGBA::a() const
 {
  return a_;
 }

 void FloatRGBA::setRed(float r)
 {
  r_ = r;
 }

 void FloatRGBA::setGreen(float g)
 {
  g_ = g;
 }

 void FloatRGBA::setBlue(float b)
 {
  b_ = b;
 }

 void FloatRGBA::setAlpha(float a)
 {
  a_ = a;
 }

};

