module;
#include <utility>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QReadWriteLock>
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
  mutable QReadWriteLock lock;
  QMap<QString, QVariant> vars;
  quint64 revisionCounter = 0;

 public:
  void loadFromOS();
  void setVariable(const QString& name, const QVariant& value);
  QVariant getVariable(const QString& name) const;
  bool hasVariable(const QString& name) const;
  QStringList variableNames() const;
  void clear();
  quint64 revision() const { return revisionCounter; }
 };

 void EnvironmentVariableManager::Impl::loadFromOS()
 {
  QWriteLocker locker(&lock);
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  for (const QString& key : env.keys()) {
   vars[key] = env.value(key);
  }
  ++revisionCounter;
 }

 void EnvironmentVariableManager::Impl::setVariable(const QString& name, const QVariant& value)
 {
  QWriteLocker locker(&lock);
  vars[name] = value;
  ++revisionCounter;
 }

 QVariant EnvironmentVariableManager::Impl::getVariable(const QString& name) const
 {
  QReadLocker locker(&lock);
  return vars.value(name);
 }

 bool EnvironmentVariableManager::Impl::hasVariable(const QString& name) const
 {
  QReadLocker locker(&lock);
  return vars.contains(name);
 }

 QStringList EnvironmentVariableManager::Impl::variableNames() const
 {
  QReadLocker locker(&lock);
  return vars.keys();
 }

 void EnvironmentVariableManager::Impl::clear()
 {
  QWriteLocker locker(&lock);
  vars.clear();
  ++revisionCounter;
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

 quint64 EnvironmentVariableManager::revision() const
 {
  return impl_->revision();
 }

};
