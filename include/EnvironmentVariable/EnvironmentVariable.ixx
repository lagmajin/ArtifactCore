// ReSharper disable All
module;
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
  void setVariable(const QString& name, const QVariant& value);
 };


}