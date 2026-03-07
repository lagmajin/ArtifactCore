module;
#include <QSettings>
#include <QCoreApplication> // QApplication::arguments() のために必要
#include <QProcessEnvironment> // OS環境変数のため
#include <QRegularExpressionMatch>
#include <QSet> 
#include <wobjectimpl.h>
module EnvironmentVariable;



namespace ArtifactCore
{
 W_OBJECT_IMPL(EnvironmentVariableManager)

 class EnvironmentVariableManager::Impl {
 private:
  QMap<QString, QVariant> vars;

 public:
  void loadFromOS();
  void setVariable(const QString& name, const QVariant& value);
  QVariant getVariable(const QString& name) const;
 };

 void EnvironmentVariableManager::Impl::loadFromOS()
 {
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

  for (const QString& key : env.keys()) {
   vars[key] = env.value(key);
  }
 }

 void EnvironmentVariableManager::Impl::setVariable(const QString& name, const QVariant& value)
 {
  vars[name] = value;
 }

 QVariant EnvironmentVariableManager::Impl::getVariable(const QString& name) const
 {
  return vars.value(name);
 }

 EnvironmentVariableManager::~EnvironmentVariableManager()
 {

 }

 EnvironmentVariableManager::EnvironmentVariableManager()
 {

 }

 void EnvironmentVariableManager::setVariable(const QString& name, const QVariant& value)
 {

 }

};