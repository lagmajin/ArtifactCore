module;
#include <utility>
#include <QSettings>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QRegularExpressionMatch>
#include <QSet> 
#include <QDebug>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>
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
  bool hasVariable(const QString& name) const;
  QStringList variableNames() const;
  void clear();
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

 bool EnvironmentVariableManager::Impl::hasVariable(const QString& name) const
 {
  return vars.contains(name);
 }

 QStringList EnvironmentVariableManager::Impl::variableNames() const
 {
  return vars.keys();
 }

 void EnvironmentVariableManager::Impl::clear()
 {
  vars.clear();
 }

 EnvironmentVariableManager::~EnvironmentVariableManager()
 {
  delete impl_;
 }

 EnvironmentVariableManager::EnvironmentVariableManager() : impl_(new Impl())
 {
  impl_->loadFromOS();
 }

 EnvironmentVariableManager* EnvironmentVariableManager::instance()
 {
  static EnvironmentVariableManager inst;
  return &inst;
 }

 void EnvironmentVariableManager::setVariable(const QString& name, const QVariant& value)
 {
  impl_->setVariable(name, value);
 }

 QVariant EnvironmentVariableManager::getVariable(const QString& name) const
 {
  return impl_->getVariable(name);
 }

 bool EnvironmentVariableManager::hasVariable(const QString& name) const
 {
  return impl_->hasVariable(name);
 }

 QStringList EnvironmentVariableManager::variableNames() const
 {
  return impl_->variableNames();
 }

 void EnvironmentVariableManager::loadFromSystemEnvironment()
 {
  impl_->loadFromOS();
 }

 void EnvironmentVariableManager::clear()
 {
  impl_->clear();
 }

};
