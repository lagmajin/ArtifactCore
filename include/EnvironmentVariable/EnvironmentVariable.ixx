module;
#include <utility>
// ReSharper disable All
#include "../Define/DllExportMacro.hpp"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <wobjectdefs.h>

export module EnvironmentVariable;
export namespace ArtifactCore {

 class LIBRARY_DLL_API EnvironmentVariableManager :public QObject{
	W_OBJECT(EnvironmentVariableManager)
 private:
  class Impl;
  Impl* impl_;
 public:
  EnvironmentVariableManager();
  ~EnvironmentVariableManager();

  EnvironmentVariableManager(const EnvironmentVariableManager&) = delete;
  EnvironmentVariableManager& operator=(const EnvironmentVariableManager&) = delete;

  static EnvironmentVariableManager* instance();

  void setVariable(const QString& name, const QVariant& value);
  QVariant getVariable(const QString& name) const;
  bool hasVariable(const QString& name) const;
  QStringList variableNames() const;
  void loadFromSystemEnvironment();
  void clear();
 };


}
