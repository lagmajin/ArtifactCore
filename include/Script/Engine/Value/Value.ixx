

module;

#include "../../../Define/DllExportMacro.hpp"
export module Script.Engine.Value;

import std;

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

 // 配列型は Value を shared_ptr で持つ
 using ValueArray = std::vector<std::shared_ptr<Value>>;
 using FunctionCallback = std::function<std::shared_ptr<Value>(const ValueArray&)>;

 // Value クラス
 struct LIBRARY_DLL_API Value {
  ValueType type;

  // 値の本体は variant で管理
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

  // 型チェック
  bool isNull() const { return type == ValueType::Null; }
  bool isNumber() const { return type == ValueType::Number; }
  bool isString() const { return type == ValueType::String; }
  bool isArray() const { return type == ValueType::Array; }
  bool isFunction() const { return type == ValueType::Function; }

  // 値取得（型安全に）
  double asNumber() const { return std::get<double>(data); }
  const std::string& asString() const { return std::get<std::string>(data); }
  const ValueArray& asArray() const { return std::get<ValueArray>(data); }
  const FunctionCallback& asFunction() const { return std::get<FunctionCallback>(data); }
 };
};