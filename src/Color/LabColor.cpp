module;

module Color.Lab;

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

 }

 void LabColor::Impl::setA(float a)
 {

 }

 void LabColor::Impl::setB(float b)
 {

 }

 LabColor::LabColor() :impl_(new Impl())
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

 }

 void LabColor::setB(float b)
 {

 }

};