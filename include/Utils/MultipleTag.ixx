module;
#include <QString>
#include <QStringList>
#include <QSet>
#include "../Define/DllExportMacro.hpp"

export module Utils.MultipleTag;

import std;

export namespace ArtifactCore {

 class LIBRARY_DLL_API MultipleTag final {
 private:
  class Impl;
  Impl* impl_;
 public:
  MultipleTag();
  MultipleTag(const MultipleTag& other);
  MultipleTag(MultipleTag&& other) noexcept;
  ~MultipleTag();

  MultipleTag& operator=(const MultipleTag& other);
  MultipleTag& operator=(MultipleTag&& other) noexcept;

  // タグの追加・削除
  void addTag(const QString& tag);
  void addTags(const QStringList& tags);
  void removeTag(const QString& tag);
  void removeTags(const QStringList& tags);
  void clear();

  // タグの検索・チェック
  bool hasTag(const QString& tag) const;
  bool hasAnyTag(const QStringList& tags) const;
  bool hasAllTags(const QStringList& tags) const;
  int count() const;
  bool isEmpty() const;

  // タグの取得
  QStringList tags() const;
  QStringList sortedTags() const;

  // フィルタリング
  QStringList filterByPrefix(const QString& prefix) const;
  QStringList filterByPattern(const QString& pattern) const;

  // 集合演算
  MultipleTag unite(const MultipleTag& other) const;      // 和集合
  MultipleTag intersect(const MultipleTag& other) const;  // 積集合
  MultipleTag subtract(const MultipleTag& other) const;   // 差集合

  // ユーティリティ
  QString toString(const QString& separator = ", ") const;
  void fromString(const QString& str, const QString& separator = ",");

  // 比較
  bool operator==(const MultipleTag& other) const;
  bool operator!=(const MultipleTag& other) const;
 };

 

};
