module;

module Color.Lab;

import std;

namespace ArtifactCore
{


 class LabColor::Impl
 {
 private:
  float L_;
  float a_;
  float b_;
 public:
  Impl();
  Impl(float L, float a, float b);
  ~Impl();
  float L() const;
  float a() const;
  float b() const;
  void setL(float L);
  void setA(float a);
  void setB(float b);
 };

 LabColor::Impl::Impl() : L_(0.0f), a_(0.0f), b_(0.0f)
 {

 }

 LabColor::Impl::Impl(float L, float a, float b):L_(L), a_(a), b_(b)
 {
 }

 LabColor::Impl::~Impl()
 {

 }

 float LabColor::Impl::L() const
 {
  return L_;
 }

 float LabColor::Impl::a() const
 {
  return a_;
 }

 float LabColor::Impl::b() const
 {
  return b_;
 }

 void LabColor::Impl::setL(float L)
 {
  L_ = L;
 }

 void LabColor::Impl::setA(float a)
 {
  a_ = a;
 }

 void LabColor::Impl::setB(float b)
 {
  b_ = b;
 }

 LabColor::LabColor() :impl_(new Impl())
 {

 }

 LabColor::LabColor(float L, float a, float b) :impl_(new Impl(L,a,b))
 {
 }

 LabColor::LabColor(const LabColor& color) :impl_(new Impl())
 {
 }

 LabColor::~LabColor()
 {
  delete impl_;
 }

 void LabColor::setL(float L)
 {
  impl_->setL(L);
 }

 void LabColor::setA(float a)
 {
  impl_->setA(a);
 }

 void LabColor::setB(float b)
 {
  impl_->setB(b);
 }

 LabColor& LabColor::operator=(const LabColor& other)
 {
  return *this;
 }
};