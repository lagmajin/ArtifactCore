module;
#include "../Define/DllExportMacro.hpp"
export module TimeCodeRange;

import std;
import Time.Code;
import Time.Rational;

export namespace ArtifactCore
{
	
	
 class LIBRARY_DLL_API TimeCodeRange
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  TimeCodeRange();
  TimeCodeRange(const TimeCode& start, const TimeCode& stop);
  TimeCodeRange(const TimeCodeRange& other);
  TimeCodeRange(TimeCodeRange&& other) noexcept;
  ~TimeCodeRange();

  TimeCodeRange& operator=(const TimeCodeRange& other);
  TimeCodeRange& operator=(TimeCodeRange&& other) noexcept;

  // ---- Setters ----
  void setStartTimeCode(const TimeCode& timecode);
  void setStopTimeCode(const TimeCode& timecode);
  void setByFrameRange(int startFrame, int stopFrame, double fps);

  // ---- Getters ----
  TimeCode startTimeCode() const;
  TimeCode stopTimeCode() const;
  double fps() const;

  // ---- åvéZ ----
  int durationFrames() const;
  double durationSeconds() const;
  RationalTime durationRational() const;

  // ---- îªíË ----
  bool containsFrame(int frame) const;
  bool contains(const TimeCode& tc) const;
  bool overlaps(const TimeCodeRange& other) const;
  bool isValid() const;

  // ---- ëÄçÏ ----
  void offset(int frames);
  void trimStart(int frames);
  void trimEnd(int frames);
  TimeCodeRange intersection(const TimeCodeRange& other) const;
  TimeCodeRange united(const TimeCodeRange& other) const;

  // ---- î‰är ----
  bool operator==(const TimeCodeRange& other) const;
  bool operator!=(const TimeCodeRange& other) const;
 };








};