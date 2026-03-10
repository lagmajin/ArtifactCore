module;

#include <QString>
#include <vector>
#include <memory>
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
export module Property.Group;




import Property.Abstract;

export namespace ArtifactCore {

using AbstractPropertyPtr = std::shared_ptr<AbstractProperty>;

class LIBRARY_DLL_API PropertyGroup {
public:
  explicit PropertyGroup(QString name = QString{});
  PropertyGroup(const PropertyGroup& other);
  PropertyGroup(PropertyGroup&& other) noexcept;
  PropertyGroup& operator=(const PropertyGroup& other);
  PropertyGroup& operator=(PropertyGroup&& other) noexcept;
  ~PropertyGroup();

  QString name() const;
  void setName(const QString& name);

  void addProperty(const AbstractPropertyPtr& property);
  bool removeProperty(const QString& propertyName);
  AbstractPropertyPtr findProperty(const QString& propertyName) const;
  size_t propertyCount() const;

  /// @brief 全プロパティを追加順に返す。
  std::vector<AbstractPropertyPtr> allProperties() const;

  /// @brief 全プロパティを displayPriority の昇順（小さい値が先頭）でソートして返す。
  /// 表示用にUI側から呼び出すことを想定。
  std::vector<AbstractPropertyPtr> sortedProperties() const;

  /// @brief 優先度を指定してプロパティを追加する便利メソッド。
  /// addProperty(プロパティ) + setDisplayPriority() のショートカット。
  /// @param property 追加するプロパティ
  /// @param priority  表示優先度（小さい値が先頭）
  void addPropertyWithPriority(const AbstractPropertyPtr& property, int priority);

private:
  class Impl;
  Impl* impl_;
  QString name_;
};

}
