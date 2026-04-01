module ;
// ReSharper disable All
#include <QObject>
#include <wobjectdefs.h>

#include <QJsonObject>

#include "../Define/DllExportMacro.hpp"
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