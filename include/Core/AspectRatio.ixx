module;
#define QT_NO_KEYWORDS
#include <QList>
#include <QString>

#include "../Define/DllExportMacro.hpp"
export module Core.AspectRatio;

import std;
import Utils.String.UniString;

export namespace ArtifactCore {
 
 class LIBRARY_DLL_API AspectRatio {
 private:
  class Impl;
  Impl* impl_;


 public:
  AspectRatio() = default;
  AspectRatio(int width, int height);
  ~AspectRatio();
  // 浮動小数点数（1.777...）を返す
  double ratio() const;

  // "16:9" のような文字列を返す
  UniString toString() const;
  void setFromString(const UniString& str);

  // 最大公約数（GCD）で約分する
  void simplify();
 };


};