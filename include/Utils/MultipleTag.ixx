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

  // ^O̒ǉE폜
  void addTag(const QString& tag);
  void addTags(const QStringList& tags);
  void removeTag(const QString& tag);
  void removeTags(const QStringList& tags);
  void clear();

  // ^ǑE`FbN
  bool hasTag(const QString& tag) const;
  bool hasAnyTag(const QStringList& tags) const;
  bool hasAllTags(const QStringList& tags) const;
  int count() const;
  bool isEmpty() const;

  // ^O̎擾
  QStringList tags() const;
  QStringList sortedTags() const;

  // tB^O
  QStringList filterByPrefix(const QString& prefix) const;
  QStringList filterByPattern(const QString& pattern) const;

  // WZ
  MultipleTag unite(const MultipleTag& other) const;      // aW
  MultipleTag intersect(const MultipleTag& other) const;  // ϏW
  MultipleTag subtract(const MultipleTag& other) const;   // W

  // [eBeB
  QString toString(const QString& separator = ", ") const;
  void fromString(const QString& str, const QString& separator = ",");

  // r
  bool operator==(const MultipleTag& other) const;
  bool operator!=(const MultipleTag& other) const;
 };

 

};
