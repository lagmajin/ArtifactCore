module;

export module Time;


import  Frame.Time;

namespace ArtifactCore {

 class Time {
 public:
  explicit Time(double seconds = 0.0);

  double seconds() const;
  int milliseconds() const;

  void setSeconds(double sec);
  void setMilliseconds(int ms);

  FrameTime toFrameTime(double fps) const;

  Time operator+(const Time& other) const;
  Time operator-(const Time& other) const;
  Time& operator+=(const Time& other);
  Time& operator-=(const Time& other);

  bool operator==(const Time& other) const;
  bool operator<(const Time& other) const;

 private:
  double sec_;
 };


}


