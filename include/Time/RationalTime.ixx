module;
#include <utility>
#include <QTime>
#include "../Define/DllExportMacro.hpp"
export module Time.Rational;

export namespace ArtifactCore {

 class LIBRARY_DLL_API RationalTime {
 private:
  class Impl;
  Impl* impl_;
 public:
  RationalTime();
  RationalTime(int64_t value,int64_t scale);
  RationalTime(const RationalTime& other);
  RationalTime& operator=(const RationalTime& other);
  ~RationalTime();
  int64_t value() const;
  int64_t scale() const;
  double toSeconds() const;
  // w肵XP[ifpsȂǁjɕϊۂvalueԂ
  int64_t rescaledTo(int64_t newScale) const;

  // --- ZqI[o[[h (Ԃ̌vZɕK{) ---
  RationalTime operator+(const RationalTime& other) const;
  RationalTime operator-(const RationalTime& other) const;
  bool operator<(const RationalTime& other) const;
  bool operator>(const RationalTime& other) const;
  bool operator<=(const RationalTime& other) const;
  bool operator>=(const RationalTime& other) const;
  bool operator==(const RationalTime& other) const;
  bool operator!=(const RationalTime& other) const;
  // ldoubleibPʁjɕϊ
  double toDouble() const;
  // --- [eBeB ---
  // b琶 (œK؂scaleݒA: 100000)
  static RationalTime fromSeconds(double seconds);
 };


};
