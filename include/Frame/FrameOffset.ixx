module;

export module Frame.Offset;

import std;
import Frame.Rate;


export namespace ArtifactCore {

 class FrameOffset
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  FrameOffset();
  FrameOffset(int offset);
  ~FrameOffset();

  // Getter and setter
  int value() const;
  void setValue(int offset);

  // Arithmetic operators
  FrameOffset operator+(const FrameOffset& other) const;
  FrameOffset operator-(const FrameOffset& other) const;
  FrameOffset operator+(int frames) const;
  FrameOffset operator-(int frames) const;
  FrameOffset& operator+=(const FrameOffset& other);
  FrameOffset& operator-=(const FrameOffset& other);
  FrameOffset& operator+=(int frames);
  FrameOffset& operator-=(int frames);

  // Comparison operators
  bool operator==(const FrameOffset& other) const;
  bool operator!=(const FrameOffset& other) const;
  bool operator<(const FrameOffset& other) const;
  bool operator<=(const FrameOffset& other) const;
  bool operator>(const FrameOffset& other) const;
  bool operator>=(const FrameOffset& other) const;

  // Utility
  bool isZero() const;
  FrameOffset abs() const;
  std::string toString() const;
  static FrameOffset fromString(const std::string& str);

  // Relation with other frame classes
  double toTimeSeconds(const FrameRate& rate) const;
  static FrameOffset fromTimeSeconds(double seconds, const FrameRate& rate);
 };



};