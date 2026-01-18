module ;

module Color.Saturation;

import std;

namespace ArtifactCore
{


 class Saturation::Impl
 {
 private:
  std::mutex mutex_;
 public:
  Impl();
  ~Impl();
  //thread safe value
  float value_{ 0.0f };
   
 };

 Saturation::Impl::Impl()
 {

 }

 Saturation::Impl::~Impl()
 {

 }

 Saturation::~Saturation()
 {
  delete impl_;
 }
 

 Saturation::Saturation() :impl_(new Impl())
 {
  
 }

 float Saturation::saturation() const 
{
   
  return impl_->value_;
 }

 void Saturation::setSaturation(float s)
 {
  float constrained = std::clamp(s, -1.0f, 1.0f);
   
  impl_->value_ = constrained;
 }

 bool Saturation::operator==(const Saturation& other) const
 {
  //floatの誤差を考慮した比較
  const float epsilon = 1e-6f;

  return std::abs(saturation() - other.saturation()) < epsilon;
 }

 bool Saturation::operator!=(const Saturation& other) const
 {
  return !(*this == other);
 }

}