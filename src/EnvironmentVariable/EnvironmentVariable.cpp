module;
#include <QSettings>
#include <QCoreApplication> // QApplication::arguments() �̂��߂ɕK�v
#include <QProcessEnvironment> // OS���ϐ��̂���
#include <QRegularExpressionMatch>
#include <QSet> 
#include <wobjectimpl.h>
module EnvironmentVariable;



namespace ArtifactCore
{
 W_OBJECT_IMPL(EnvironmentVariableManager)

 class EnvironmentVariableManager::Impl {
 private:


 public:
  void loadFromOS();
 };

 void EnvironmentVariableManager::Impl::loadFromOS()
 {

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