module;

#include <QString>
#include <vector>
#include <memory>
#include "../Define/DllExportMacro.hpp"

export module Property.Group;

import std;
import Property.Abstract;

export namespace ArtifactCore {

using AbstractPropertyPtr = std::shared_ptr<AbstractProperty>;

class LIBRARY_DLL_API PropertyGroup {
public:
  explicit PropertyGroup(QString name = QString{});
  ~PropertyGroup();

  QString name() const;
  void setName(const QString& name);

  void addProperty(const AbstractPropertyPtr& property);
  bool removeProperty(const QString& propertyName);
  AbstractPropertyPtr findProperty(const QString& propertyName) const;
  size_t propertyCount() const;
  std::vector<AbstractPropertyPtr> allProperties() const;

private:
  class Impl;
  Impl* impl_;
  QString name_;
};

}