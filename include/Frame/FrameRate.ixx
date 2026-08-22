module;
// ReSharper disable All


#include "../Define/DllExportMacro.hpp"
#include <cstdint>

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QtCore/QString>
#include <QtCore/QJsonObject>
export module Frame.Rate;

import Utils.String.UniString;

export namespace ArtifactCore {

 enum class eFramerateStringFormat {

 };
 enum class eFamousFrameRate {
  fps15_0,
  fps_24_0,
  fps23_976,
  fps29_97,
  fps30_0,
  fps59_94,
  fps60_0,
 };


 class LIBRARY_DLL_API FrameRate final {
 private:
  class Impl;
  Impl* impl_;
 public:
  FrameRate();
  FrameRate(float frameRate);
  FrameRate(const QString& str);
  FrameRate(const FrameRate& frameRate);
  FrameRate(FrameRate&& framerate)noexcept;
  virtual ~FrameRate();
  float framerate() const;
  void setFrameRate(float frame = 30.0f);

  // Exact rational rate (e.g. 30000/1001). When set, framerate() reports the
  // converted value and timecode/rate math can avoid float drift.
  static FrameRate fromRational(std::int64_t numerator, std::int64_t denominator);
  void setRationalRate(std::int64_t numerator, std::int64_t denominator);
  std::int64_t numerator() const;
  std::int64_t denominator() const;
  bool hasExactRational() const;
  double exactFps() const;

  UniString toString() const;
  void setFromString(const QString& framerate);
  bool hasDropframe() const;
  void speedUp(float frame = 1.0f);
  void speedDown(float frame = 1.0f);

  void swap(FrameRate& other) noexcept;

  QJsonObject toJson() const;
  void setFromJson(const QJsonObject& object);
  void readFromJson(QJsonObject& object) const;
  void writeToJson(QJsonObject& object) const;

  UniString toDisplayString(bool includeDropframe = true) const; // UI向け表示
  static FrameRate fromJsonStatic(const QJsonObject& obj);

  FrameRate& operator=(float rate);
  FrameRate& operator=(const QString& str);
  FrameRate& operator=(const FrameRate& framerate);
  FrameRate& operator=(FrameRate&& framerate) noexcept;
 };

 bool operator==(const FrameRate& framerate1, const FrameRate& framerate2);
 bool operator!=(const FrameRate& framerate1, const FrameRate& framerate2);


 class FrameRateOffsetPrivate;

 class FrameRateOffset {
 private:
  std::int64_t value_ = 0;

 public:
  explicit FrameRateOffset(std::int64_t value = 0);
  ~FrameRateOffset() = default;
  std::int64_t value() const { return value_; }
  void setValue(std::int64_t value) { value_ = value; }
 };

 bool operator==(const FrameRateOffset& offset1, const FrameRateOffset& offset2);
 bool operator!=(const FrameRateOffset& offset1, const FrameRateOffset& offset2);
 bool operator<=(const FrameRateOffset& offset1, const FrameRateOffset& offset2);








};
