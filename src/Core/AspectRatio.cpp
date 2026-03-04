module;

module Core.AspectRatio;

import std;


namespace ArtifactCore {

 class AspectRatio::Impl {
 private:
 
 public:
  Impl();
  Impl(int w, int h);
  ~Impl();
  int w_ = 1;
  int h_ = 1;
 };

 AspectRatio::Impl::Impl()
 {

 }

 AspectRatio::Impl::Impl(int w, int h) : w_(w), h_(h)
 {

 }

 AspectRatio::Impl::~Impl()
 {

 }

 void AspectRatio::simplify()
 {
  if (impl_->w_ == 0 || impl_->h_ == 0) return;
  int common = std::gcd(impl_->w_, impl_->h_);
  if (common == 0) return;
  impl_->w_ /= common;
  impl_->h_ /= common;
 }

 UniString AspectRatio::toString() const
 {
  return QString("%1:%2").arg(impl_->w_).arg(impl_->h_);
 }

 double AspectRatio::ratio() const
 {
  return static_cast<double>(impl_->w_) / impl_->h_;
 }

 AspectRatio::AspectRatio() : impl_(new Impl())
 {

 }

 AspectRatio::AspectRatio(int width, int height) : impl_(new Impl(width, height))
 {
  simplify();
 }

 AspectRatio::~AspectRatio()
 {
  delete impl_;
 }

 void AspectRatio::setFromString(const UniString& str)
 {

 }

};