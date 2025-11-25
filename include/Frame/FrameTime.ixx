module;
export module Frame.Time;



export namespace ArtifactCore {

 class Time;

 class FrameTime {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit FrameTime(int frame = 0);

  int frame() const;
  void setFrame(int f);

  Time toTime(double fps) const;

  FrameTime operator+(int delta) const;
  FrameTime operator-(int delta) const;
  FrameTime& operator+=(int delta);
  FrameTime& operator-=(int delta);

  bool operator==(const FrameTime& other) const;
  bool operator<(const FrameTime& other) const;


 };


}