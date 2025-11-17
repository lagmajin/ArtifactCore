module;

module Time.Code;

import std;

namespace ArtifactCore {

 class TimeCode::Impl {
 private:

 public:
  Impl();
  ~Impl();
  int frame = 0;
  double fps = 30.0;
 };

 TimeCode::Impl::Impl()
 {

 }

 TimeCode::Impl::~Impl()
 {

 }

 TimeCode::TimeCode() :impl_(new Impl())
 {

 }

 TimeCode::TimeCode(int frame, double fps) :impl_(new Impl())
 {

 }

 TimeCode::TimeCode(int h, int m, int s, int f, double fps) :impl_(new Impl())
 {

 }

 TimeCode::~TimeCode()
 {
  delete impl_;
 }

 void TimeCode::setByFrame(int frame)
 {

 }

 void TimeCode::setByHMSF(int h, int m, int s, int f)
 {

 }

 void TimeCode::toHMSF(int& h, int& m, int& s, int& f) const
 {

 }

 std::string TimeCode::toStdString() const
 {

  return std::string();
 }

}