module;
#include <QVector>
#include <QString>
export module Utils.Convertor.String;

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




export namespace ArtifactCore {

 inline std::string toStdString(const QString& qstr) {
  QByteArray utf8 = qstr.toUtf8(); // Qt UTF-8ϊ
  return std::string(utf8.constData(), utf8.size());
 }

 inline std::u16string toStdU16String(const QString& qstr) {
  std::u16string out;
  out.reserve(qstr.size());
  for (QChar qc : qstr) {
   out.push_back(static_cast<char16_t>(qc.unicode()));
  }
  return out;
 }

 inline std::u32string toStdU32String(const QString& qstr) {
  // Qt  UCS-4 𐶐  ܂ UTF-32-safe
  QVector<uint> ucs4 = qstr.toUcs4();

  std::u32string out;
  out.reserve(ucs4.size());
  for (uint codepoint : ucs4) {
   out.push_back(static_cast<char32_t>(codepoint));
  }
  return out;
 }

};