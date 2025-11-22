module;
#include <QString>
export module UniString;

import std;

export namespace ArtifactCore {
 
 class UniString {
 private:
  class Impl;
  Impl* impl_;
 public:
  UniString();
  UniString(const UniString& other);
  ~UniString();

  std::u16string toStdU16String() const;

  QString toQString() const;
  void setQString(const QString& str);
  UniString& operator=(const UniString& other);
  UniString& operator=(UniString&& other) noexcept;
 };


};