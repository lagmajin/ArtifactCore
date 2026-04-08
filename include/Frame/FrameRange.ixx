module;

#include <vector>
#include "../Define/DllExportMacro.hpp"

#include <QtCore/QString>
#include <QtCore/QJsonObject>

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
export module Frame.Range;
import Frame.Position;
import Frame.Offset;
import Frame.Rate;

export namespace ArtifactCore {

 // t[͈͂ NX
 // ҏWɂ^CC͈̔́A[NGAAC[̎ԂȂǂ
 class LIBRARY_DLL_API FrameRange {
 private:
  class Impl;
  Impl* impl_;
  
 public:
  // RXgN^
  FrameRange();
  FrameRange(int64_t start, int64_t end);
  FrameRange(const FramePosition& start, const FramePosition& end);
  FrameRange(const FrameRange& other);
  FrameRange(FrameRange&& other) noexcept;
  ~FrameRange();

  // Zq
  FrameRange& operator=(const FrameRange& other);
  FrameRange& operator=(FrameRange&& other) noexcept;

  // ͈͐ݒ
  void setStart(int64_t start);
  void setEnd(int64_t end);
  void setRange(int64_t start, int64_t end);
  void setDuration(int64_t duration);  // Jn_͕ςɒݒ

  // ͈͎擾
  int64_t start() const;
  int64_t end() const;
  int64_t duration() const;  // length() ̃GCAX
  int64_t length() const;
  
  FramePosition startPosition() const;
  FramePosition endPosition() const;

  // 
  bool isValid() const;  // start <= end
  bool isEmpty() const;  // start == end
  bool isInfinite() const;

  // ͈̓`FbN
  bool contains(int64_t frame) const;
  bool contains(const FramePosition& position) const;
  bool contains(const FrameRange& other) const;  // Sɕ܂邩
  
  bool overlaps(const FrameRange& other) const;  // dȂ肪邩
  bool touches(const FrameRange& other) const;   // ڐGĂ邩idȂ܂͗אځj

  // ͈͑
  void expand(int64_t frames);  // [g
  void expandStart(int64_t frames);
  void expandEnd(int64_t frames);
  
  void shrink(int64_t frames);  // [k
  void shrinkStart(int64_t frames);
  void shrinkEnd(int64_t frames);
  
  void shift(int64_t frames);  // ͈͑Ŝړ
  void shift(const FrameOffset& offset);
  
  FrameRange shifted(int64_t frames) const;  // ړRs[Ԃ
  FrameRange expanded(int64_t frames) const;  // gRs[Ԃ
  FrameRange shrinked(int64_t frames) const;  // kRs[Ԃ

  // ͈͉Z
  FrameRange united(const FrameRange& other) const;      // aWi܂ޔ͈́j
  FrameRange intersected(const FrameRange& other) const; // ϏWidȂ蕔j
  bool intersects(const FrameRange& other, FrameRange& result) const;  // ϏW݂邩

  // NbsO
  void clip(const FrameRange& bounds);  // w͈͓ɐ
  FrameRange clipped(const FrameRange& bounds) const;
  
  int64_t clampFrame(int64_t frame) const;  // t[͈͓Ɋۂ߂
  FramePosition clampPosition(const FramePosition& position) const;

  // Ce[V
  class Iterator {
  private:
   int64_t current_;
   int64_t endValue_;
  public:
   Iterator(int64_t current, int64_t endValue) : current_(current), endValue_(endValue) {}
   int64_t operator*() const { return current_; }
   Iterator& operator++() { ++current_; return *this; }
   bool operator!=(const Iterator& other) const { return current_ != other.current_; }
  };

  Iterator beginIterator() const;
  Iterator endIterator() const;

  // [eBeB
  FrameRange normalized() const;  // start > end ̏ꍇAւĐK
  void normalize();

  std::vector<int64_t> frames() const;  // ͈͓̑St[ԍ擾
  std::vector<int64_t> uniformSample(int count) const;  // ϓɃTvO

  // ԕϊ
  double durationSeconds(double fps) const;
  double durationSeconds(const FrameRate& rate) const;
  
  QString toTimecode(double fps) const;  // "00:00:10:00" - "00:00:20:00"
  QString toTimecode(const FrameRate& rate) const;

  // VACY
  QJsonObject toJson() const;
  static FrameRange fromJson(const QJsonObject& json);
  
  QString toString() const;  // "[100, 200]" `
  static FrameRange fromString(const QString& str);

  // rZq
  bool operator==(const FrameRange& other) const;
  bool operator!=(const FrameRange& other) const;
  bool operator<(const FrameRange& other) const;   // start Ŕr
  bool operator<=(const FrameRange& other) const;
  bool operator>(const FrameRange& other) const;
  bool operator>=(const FrameRange& other) const;

  // Ȕ͈
  static FrameRange invalid();     // Ȕ͈
  static FrameRange infinite();    // ͈
  static FrameRange zero();        // 0͈̔
  static FrameRange fromDuration(int64_t start, int64_t duration);
 };

 // GCAX
 using WorkArea = FrameRange;      // ^CC̃[NGA
 using PlaybackRange = FrameRange; // Đ͈
 using LayerRange = FrameRange;    // C[̎

};
