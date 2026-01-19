module;


module  Frame.Offset;

import Frame.Rate;

namespace ArtifactCore
{
 class FrameOffset::Impl
 {
 private:
  int offset_ = 0;
 public:
  Impl(int offset = 0) : offset_(offset) {}
  ~Impl() {}

  int value() const { return offset_; }
  void setValue(int offset) { offset_ = offset; }
 };

 FrameOffset::FrameOffset() : impl_(new Impl())
 {

 }

 FrameOffset::FrameOffset(int offset) : impl_(new Impl(offset))
 {

 }

 FrameOffset::~FrameOffset()
 {
  delete impl_;
 }

 int FrameOffset::value() const
 {
  return impl_->value();
 }

 void FrameOffset::setValue(int offset)
 {
  impl_->setValue(offset);
 }

 FrameOffset FrameOffset::operator+(const FrameOffset& other) const
 {
  return FrameOffset(value() + other.value());
 }

 FrameOffset FrameOffset::operator-(const FrameOffset& other) const
 {
  return FrameOffset(value() - other.value());
 }

 FrameOffset FrameOffset::operator+(int frames) const
 {
  return FrameOffset(value() + frames);
 }

 FrameOffset FrameOffset::operator-(int frames) const
 {
  return FrameOffset(value() - frames);
 }

 FrameOffset& FrameOffset::operator+=(const FrameOffset& other)
 {
  setValue(value() + other.value());
  return *this;
 }

 FrameOffset& FrameOffset::operator-=(const FrameOffset& other)
 {
  setValue(value() - other.value());
  return *this;
 }

 FrameOffset& FrameOffset::operator+=(int frames)
 {
  setValue(value() + frames);
  return *this;
 }

 FrameOffset& FrameOffset::operator-=(int frames)
 {
  setValue(value() - frames);
  return *this;
 }

 bool FrameOffset::operator==(const FrameOffset& other) const
 {
  return value() == other.value();
 }

 bool FrameOffset::operator!=(const FrameOffset& other) const
 {
  return !(*this == other);
 }

 bool FrameOffset::operator<(const FrameOffset& other) const
 {
  return value() < other.value();
 }

 bool FrameOffset::operator<=(const FrameOffset& other) const
 {
  return value() <= other.value();
 }

 bool FrameOffset::operator>(const FrameOffset& other) const
 {
  return value() > other.value();
 }

 bool FrameOffset::operator>=(const FrameOffset& other) const
 {
  return value() >= other.value();
 }

 bool FrameOffset::isZero() const
 {
  return value() == 0;
 }

 FrameOffset FrameOffset::abs() const
 {
  return FrameOffset(std::abs(value()));
 }

 std::string FrameOffset::toString() const
 {
  return std::to_string(value());
 }

 FrameOffset FrameOffset::fromString(const std::string& str)
 {
  try {
    int val = std::stoi(str);
    return FrameOffset(val);
  } catch (...) {
    return FrameOffset(0);
  }
 }

 double FrameOffset::toTimeSeconds(const FrameRate& rate) const
 {
  return static_cast<double>(value()) / rate.framerate();
 }

 FrameOffset FrameOffset::fromTimeSeconds(double seconds, const FrameRate& rate)
 {
  int frames = static_cast<int>(seconds * rate.framerate());
  return FrameOffset(frames);
 }

};
