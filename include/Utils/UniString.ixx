module;
#include <QString>

#include "../Define/DllExportMacro.hpp"

export module Utils.UniString;

import std;

export namespace ArtifactCore {

 class LIBRARY_DLL_API UniString {
 private:
  class Impl;
  Impl* impl_;
 public:
  UniString();
  UniString(const UniString& other);
  ~UniString();
  size_t length() const;
  std::u16string toStdU16String() const;
  std::u32string toStdU32String() const;
  operator QString() const;
  operator std::u16string() const;
  QString toQString() const;
  
  void setQString(const QString& str);
  UniString& operator=(const UniString& other);
  UniString& operator=(UniString&& other) noexcept;
 };


};