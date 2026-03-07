module;
#include "../Define/DllExportMacro.hpp"
export module TimeCodeRange;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



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

  // ---- vZ ----
  int durationFrames() const;
  double durationSeconds() const;
  RationalTime durationRational() const;

  // ----  ----
  bool containsFrame(int frame) const;
  bool contains(const TimeCode& tc) const;
  bool overlaps(const TimeCodeRange& other) const;
  bool isValid() const;

  // ----  ----
  void offset(int frames);
  void trimStart(int frames);
  void trimEnd(int frames);
  TimeCodeRange intersection(const TimeCodeRange& other) const;
  TimeCodeRange united(const TimeCodeRange& other) const;

  // ---- r ----
  bool operator==(const TimeCodeRange& other) const;
  bool operator!=(const TimeCodeRange& other) const;
 };








};