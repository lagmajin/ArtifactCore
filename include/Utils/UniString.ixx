module;
#include <QString>

#include "../Define/DllExportMacro.hpp"

export module Utils.String.UniString;

import std;

export namespace ArtifactCore {

 class LIBRARY_DLL_API UniString {
 private:
  class Impl;
  Impl* impl_;
 public:
  UniString();
  UniString(const UniString& other);
  UniString(const std::u16string& u16);
  UniString(const QString& str);
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

  // Comparison operators for lexicographical ordering
  bool operator==(const UniString& other) const;
  bool operator!=(const UniString& other) const;
  bool operator<(const UniString& other) const;
  bool operator<=(const UniString& other) const;
  bool operator>(const UniString& other) const;
  bool operator>=(const UniString& other) const;
 };


};