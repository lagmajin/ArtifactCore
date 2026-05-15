module;
#include <QDir>
#include <QString>
#include "../Define/DllExportMacro.hpp"
//#include <opencv2/core/cvdef.h>
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
export module Time.Code;

import Utils.String.UniString;
import Time.Rational;

export namespace ArtifactCore {

 class LIBRARY_DLL_API TimeCode final {
 private:
  class Impl;
  Impl* impl_;

 public:
  TimeCode();
  TimeCode(int frame, double fps);
  TimeCode(int h, int m, int s, int f, double fps);
  ~TimeCode();
  TimeCode(const TimeCode&) = delete;
  TimeCode& operator=(const TimeCode&) = delete;

  TimeCode(TimeCode&&) noexcept;
  TimeCode& operator=(TimeCode&&) noexcept;

  void setByFrame(int frame);
  void setByHMSF(int h, int m, int s, int f);

  // ---- getters ----
  int frame() const;
  double  fps() const;

  void toHMSF(int& h, int& m, int& s, int& f) const;
  double toSeconds() const;

  std::string toStdString() const;
  QString toString() const;

  void setFromQString(const QString& str);

  // ---- RationalTime連携 ----
  // TimeCodeをRationalTimeに変換（フレーム精度を保持）
  RationalTime toRationalTime() const;
  // RationalTimeからTimeCodeを生成
  static TimeCode fromRationalTime(const RationalTime& rt, double fps);
  // RationalTimeで時間を設定
  void setFromRationalTime(const RationalTime& rt);
 };



};
