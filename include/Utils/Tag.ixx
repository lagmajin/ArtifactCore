module;
#define QT_NO_KEYWORDS
#include <QString>
export module Utils.Tag;

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



import Utils.String.UniString;
import Utils.String.Like;

namespace ArtifactCore {

 export class Tag final {
 private:
  class Impl;
  Impl* impl_;
 public:
  Tag();
  Tag(const Tag& other);
  Tag(Tag&& other) noexcept;
  Tag(const UniString& name);
  ~Tag();
  UniString name() const;

  template<StringLike S>
  void setName(const S& name);
  void setName(const UniString& name);
  Tag& operator=(const Tag& other);
  Tag& operator=(Tag&& other) noexcept;
 };

 template<StringLike S>
 void ArtifactCore::Tag::setName(const S& v)
 {

 }

};