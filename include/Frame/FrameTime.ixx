module;
#include <utility>
export module Frame.Time;

import Time.Rational;

export namespace ArtifactCore {

 class FrameTime {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit FrameTime(int frame = 0);
  FrameTime(const FrameTime& other);
  FrameTime(FrameTime&& other) noexcept;
  FrameTime& operator=(const FrameTime& other);
  FrameTime& operator=(FrameTime&& other) noexcept;
  ~FrameTime();

  int frame() const;
  void setFrame(int f);

  RationalTime toTime(double fps) const;

  FrameTime operator+(int delta) const;
  FrameTime operator-(int delta) const;
  FrameTime& operator+=(int delta);
  FrameTime& operator-=(int delta);

  bool operator==(const FrameTime& other) const;
  bool operator<(const FrameTime& other) const;


 };


}
