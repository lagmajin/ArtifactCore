module;
#include <QObject>
#include <wobjectdefs.h>

#include <QJsonObject>
export module EnvironmentVariable;


export namespace ArtifactCore {

 class EnvironmentVariableManager :public QObject{
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