module;
#include <QString>
export module Frame.Position;

import std;

import <cstdint>;

export namespace ArtifactCore {

 

 class FramePosition {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit FramePosition(int framePosition=0);
  FramePosition(const FramePosition& other);
  FramePosition(FramePosition&& other) noexcept;
  ~FramePosition();

  int64_t framePosition() const;

  bool isValid() const;
  bool isNegative() const;

  double toSeconds(double fps) const;
  static FramePosition fromSeconds(double seconds, double fps);

  static FramePosition min();
  static FramePosition max();

  FramePosition& operator=(const FramePosition& other);
  FramePosition& operator=(FramePosition&& other) noexcept;

  FramePosition operator+(int64_t frames) const;
  FramePosition operator-(int64_t frames) const;
  FramePosition& operator+=(int64_t frames);
  FramePosition& operator-=(int64_t frames);

  int64_t operator-(const FramePosition& other) const;

  // 比較
  bool operator==(const FramePosition&) const;
  bool operator!=(const FramePosition&) const;
  bool operator<(const FramePosition&) const;
  bool operator<=(const FramePosition&) const;
  bool operator>(const FramePosition&) const;
  bool operator>=(const FramePosition&) const;
 };


};