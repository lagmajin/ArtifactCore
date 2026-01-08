module;
#include <QTime>
#include "../Define/DllExportMacro.hpp"
export module Time.Rational;

import std;

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
  // 指定したスケール（fpsなど）に変換した際のvalueを返す
  int64_t rescaledTo(int64_t newScale) const;

  // --- 演算子オーバーロード (時間の計算に必須) ---
  RationalTime operator+(const RationalTime& other) const;
  RationalTime operator-(const RationalTime& other) const;
  bool operator<(const RationalTime& other) const;
  bool operator>(const RationalTime& other) const;
  bool operator==(const RationalTime& other) const;
  bool operator!=(const RationalTime& other) const;
  // --- ユーティリティ ---
  // 秒から生成 (内部で適切なscaleを自動設定、例: 100000等)
  static RationalTime fromSeconds(double seconds);
 };


};