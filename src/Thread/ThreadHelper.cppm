module;
#include "../../include/Define/DllExportMacro.hpp"
#include <QThread>
#include <QString>
module Thread.Helper;

import std;


namespace ArtifactCore
{

namespace {
struct SharedBackgroundThreadPoolHolder {
  SharedBackgroundThreadPoolHolder() {
    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    pool.setMaxThreadCount(std::max(1, hw > 0 ? std::min(hw, 4) : 2));
    pool.setExpiryTimeout(-1);
    pool.setObjectName(QStringLiteral("ArtifactBackgroundThreadPool"));
  }

  QThreadPool pool;
};
} // namespace

QThreadPool& sharedBackgroundThreadPool()
{
  static SharedBackgroundThreadPoolHolder holder;
  return holder.pool;
}

ScopedThreadName::ScopedThreadName(const std::string& name)
    : ScopedThreadName(QString::fromStdString(name))
{
}

ScopedThreadName::ScopedThreadName(const QString& name)
{
  if (auto* thread = QThread::currentThread()) {
    previousName_ = thread->objectName();
    thread->setObjectName(name);
  }
}

ScopedThreadName::~ScopedThreadName()
{
  if (auto* thread = QThread::currentThread()) {
    thread->setObjectName(previousName_);
  }
}

void setCurrentThreadName(const std::string& name)
 {
  if (auto* thread = QThread::currentThread()) {
    thread->setObjectName(QString::fromStdString(name));
  }
 }

};
