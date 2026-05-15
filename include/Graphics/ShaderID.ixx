module;
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Graphics.ShaderID;

export namespace ArtifactCore {

 enum class BuiltinShader
 {
  Grayscale,
  GaussianBlur,
  LuminanceExtract,
  AdditiveBlend,
  MultiplyBlend,
  ScreenBlend,
  // ... その他
 };

 class ShaderID
 {
 public:
  // 文字列からShaderIDを生成（カスタムシェーダーや動的ロード用）
  explicit ShaderID(const std::string& id_string);

  // ID文字列を取得
  const std::string& GetIdString() const;

  // 比較演算子などをオーバーロード (std::mapのキーなどとして使えるように)
  bool operator==(const ShaderID& other) const;
  bool operator!=(const ShaderID& other) const;
  // ...

 private:
  std::string m_idString;
 };

 // 内部ヘルパー (BuiltinShader enum から ShaderID へのマッピング)
 namespace ShaderRegistry {
  ShaderID GetShaderID(BuiltinShader shader_enum);
  // ユーザー入力の文字列IDを検証し、誤字を修正または提案する機能もここに実装
  ShaderID ValidateAndSuggestShaderID(const std::string& user_input_id);
 }






};
