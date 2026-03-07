module;
#define QT_NO_KEYWORDS
#include <QString>
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
module Utils.Tag;





import Utils.String.UniString;

namespace ArtifactCore {

 class Tag::Impl {
 private:

 public:
  Impl();
  ~Impl();
  UniString name;
 };

 Tag::Impl::Impl()
 {

 }

 Tag::Impl::~Impl()
 {

 }

 Tag::Tag() :impl_(new Impl())
 {

 }

 Tag::Tag(const UniString& name) :impl_(new Impl())
 {

 }

 Tag::Tag(const Tag& other) :impl_(new Impl())
 {

 }

 Tag::Tag(Tag&& other) noexcept :impl_(new Impl())
 {

 }

 Tag::~Tag()
 {
  delete impl_;
 }
 

 void Tag::setName(const UniString& name)
 {

 }

Tag& Tag::operator=(const Tag& other)
 {

 return *this;
 }

Tag& Tag::operator=(Tag&& other) noexcept
 {
 return *this;
 }

};