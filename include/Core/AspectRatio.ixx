module;
#include <memory>
#include <QString>
#include "../Define/DllExportMacro.hpp"

export module Core.AspectRatio;

import std;
import Utils.String.UniString;

export namespace ArtifactCore {

 class LIBRARY_DLL_API AspectRatio {
 public:
  AspectRatio();
  AspectRatio(int width, int height);
  ~AspectRatio();

  // Copy and Move operations
  AspectRatio(const AspectRatio& other);
  AspectRatio& operator=(const AspectRatio& other);
  AspectRatio(AspectRatio&& other) noexcept;
  AspectRatio& operator=(AspectRatio&& other) noexcept;

  double ratio() const;
  UniString toString() const;
  void setFromString(const UniString& str);
  void simplify();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
 };

}