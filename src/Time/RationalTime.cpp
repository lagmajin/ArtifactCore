module;

module Time.Rational;

import std;

namespace ArtifactCore {

 class RationalTime::Impl {
 private:
 
 public:
  Impl(); 
  Impl(int64_t value, int64_t scale);
  ~Impl();
  int64_t value_=0;
  int64_t scale_=0;
 };

 RationalTime::Impl::Impl()
 {

 }

 RationalTime::Impl::Impl(int64_t value, int64_t scale):value_(value),scale_(scale)
 {

 }

 RationalTime::Impl::~Impl()
 {

 }

 RationalTime::RationalTime():impl_(new Impl())
 {

 }

 RationalTime::RationalTime(int64_t value, int64_t scale) :impl_(new Impl(value,scale))
 {

 }

 RationalTime::RationalTime(const RationalTime& other) :impl_(new Impl())
 {

 }

 RationalTime::~RationalTime()
 {
  delete impl_;
  
 }

 int64_t RationalTime::value() const
 {
  return impl_->value_;
 }

 int64_t RationalTime::scale() const
 {
  return impl_->scale_;
 }
	
 RationalTime& RationalTime::operator=(const RationalTime& other)
 {

  return *this;
 }
	
 double RationalTime::toSeconds() const
 {

  return 0.0f;
 }

 RationalTime RationalTime::operator+(const RationalTime& other) const
 {
  return RationalTime();
 }

 RationalTime RationalTime::operator-(const RationalTime& other) const
 {

  return RationalTime();
 }

 bool RationalTime::operator<(const RationalTime& other) const
 {
  return true;
 }

 bool RationalTime::operator>(const RationalTime& other) const
 {
  return (*this) < other;
 }

 bool RationalTime::operator==(const RationalTime& other) const
 {
  return true;
 }

 bool RationalTime::operator!=(const RationalTime& other) const
 {
  return !((*this) == other);
 }

};