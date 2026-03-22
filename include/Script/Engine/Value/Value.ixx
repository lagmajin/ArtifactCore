

module;

#include "../../../Define/DllExportMacro.hpp"
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
export module Script.Engine.Value;





export namespace ArtifactCore {

 enum class ValueType {
  Null,
  Number,
  String,
  Array,
  Function
 };

 // Forward declaration
 struct Value;

 // z^ Value  shared_ptr Ŏ
 using ValueArray = std::vector<std::shared_ptr<Value>>;
 using FunctionCallback = std::function<std::shared_ptr<Value>(const ValueArray&)>;

 // Value NX
 struct LIBRARY_DLL_API Value {
  ValueType type;

  // l̖{̂ variant ŊǗ
  std::variant<
   std::monostate, // Null
   double,         // Number
   std::string,    // String
   ValueArray,     // Array
   FunctionCallback // Function
  > data;

  Value() : type(ValueType::Null), data(std::monostate{}) {}
  explicit Value(double num) : type(ValueType::Number), data(num) {}
  explicit Value(const std::string& str) : type(ValueType::String), data(str) {}
  explicit Value(const ValueArray& arr) : type(ValueType::Array), data(arr) {}
  explicit Value(const FunctionCallback& func) : type(ValueType::Function), data(func) {}

  // ^`FbN
  bool isNull() const { return type == ValueType::Null; }
  bool isNumber() const { return type == ValueType::Number; }
  bool isString() const { return type == ValueType::String; }
  bool isArray() const { return type == ValueType::Array; }
  bool isFunction() const { return type == ValueType::Function; }

  // l擾i^SɁj
  double asNumber() const { return std::get<double>(data); }
  const std::string& asString() const { return std::get<std::string>(data); }
  const ValueArray& asArray() const { return std::get<ValueArray>(data); }
  const FunctionCallback& asFunction() const { return std::get<FunctionCallback>(data); }
 };
};