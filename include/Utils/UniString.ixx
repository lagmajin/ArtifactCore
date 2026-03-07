module;
#include <QString>

#include "../Define/DllExportMacro.hpp"

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Utils.String.UniString;





export namespace ArtifactCore {

 class LIBRARY_DLL_API UniString {
 private:
  class Impl;
  Impl* impl_;
 public:
  UniString();
  UniString(const UniString& other);
  UniString(const std::u16string& u16);
  UniString(const std::string& str);
  UniString(const char* str);
  UniString(const char16_t* str);
  UniString(const char32_t* str);
  UniString(const QString& str);
  ~UniString();
  size_t length() const;
  std::u16string toStdU16String() const;
  std::u32string toStdU32String() const;
  operator QString() const;
  operator std::u16string() const;
  operator std::string() const;
  QString toQString() const;

  void setQString(const QString& str);
  UniString& operator=(const UniString& other);
  UniString& operator=(UniString&& other) noexcept;

  // Static factory method
  static UniString fromQString(const QString& str);

  // Comparison operators for lexicographical ordering
  bool operator==(const UniString& other) const;
  bool operator!=(const UniString& other) const;
  bool operator<(const UniString& other) const;
  bool operator<=(const UniString& other) const;
  bool operator>(const UniString& other) const;
  bool operator>=(const UniString& other) const;
 };


};