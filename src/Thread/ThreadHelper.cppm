module;
#include "../../include/Define/DllExportMacro.hpp"
#include <QThread>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <algorithm>
#include <map>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif
module Thread.Helper;

import std;


namespace ArtifactCore
{

namespace {
struct SharedBackgroundThreadPoolHolder {
  SharedBackgroundThreadPoolHolder() {
    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    const int cappedHw = hw > 0 ? ((hw < 4) ? hw : 4) : 2;
    pool.setMaxThreadCount(cappedHw < 1 ? 1 : cappedHw);
    pool.setExpiryTimeout(-1);
    pool.setObjectName(QStringLiteral("ArtifactBackgroundThreadPool"));
  }

  QThreadPool pool;
};

QString normalizeThreadName(QString name)
{
  static const QRegularExpression ffmpegWorkerPattern(QStringLiteral(R"(^(av:[^:]+:df)\d+$)"));
  static const QRegularExpression videoOpenPattern(QStringLiteral(R"(^(VideoLayer/open:).+$)"));

  const auto ffmpegMatch = ffmpegWorkerPattern.match(name);
  if (ffmpegMatch.hasMatch()) {
    return ffmpegMatch.captured(1) + QStringLiteral("*");
  }

  const auto videoOpenMatch = videoOpenPattern.match(name);
  if (videoOpenMatch.hasMatch()) {
    return videoOpenMatch.captured(1) + QStringLiteral("*");
  }

  return name;
}

#ifdef _WIN32
QString describeThread(HANDLE threadHandle)
{
  if (!threadHandle) {
    return {};
  }

  using GetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
  static const auto getThreadDescription =
      reinterpret_cast<GetThreadDescriptionFn>(
          GetProcAddress(GetModuleHandleW(L"Kernel32.dll"), "GetThreadDescription"));
  if (!getThreadDescription) {
    return {};
  }

  PWSTR description = nullptr;
  if (FAILED(getThreadDescription(threadHandle, &description)) || !description) {
    return {};
  }

  const QString described = QString::fromWCharArray(description).trimmed();
  LocalFree(description);
  return described;
}
#endif
} // namespace

QThreadPool& sharedBackgroundThreadPool()
{
  static SharedBackgroundThreadPoolHolder holder;
  return holder.pool;
}

BackgroundThreadPoolSnapshot sharedBackgroundThreadPoolSnapshot()
{
  auto& pool = sharedBackgroundThreadPool();
  BackgroundThreadPoolSnapshot snapshot;
  snapshot.poolName = pool.objectName();
  snapshot.maxThreadCount = pool.maxThreadCount();
  snapshot.activeThreadCount = pool.activeThreadCount();
  snapshot.expiryTimeoutMs = pool.expiryTimeout();
  return snapshot;
}

ProcessThreadSnapshot currentProcessThreadSnapshot()
{
  ProcessThreadSnapshot snapshot;

#ifdef _WIN32
  const DWORD processId = GetCurrentProcessId();
  HANDLE threadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (threadSnapshot == INVALID_HANDLE_VALUE) {
    return snapshot;
  }

  THREADENTRY32 entry{};
  entry.dwSize = sizeof(entry);
  std::map<QString, int> counts;
  if (Thread32First(threadSnapshot, &entry)) {
    do {
      if (entry.th32OwnerProcessID != processId) {
        continue;
      }

      ++snapshot.totalThreadCount;
      QString threadName = QStringLiteral("(unnamed)");
      HANDLE threadHandle = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
      if (threadHandle) {
        const QString described = describeThread(threadHandle);
        if (!described.isEmpty()) {
          threadName = described;
        }
        CloseHandle(threadHandle);
      }

      ++counts[normalizeThreadName(threadName)];
    } while (Thread32Next(threadSnapshot, &entry));
  }
  CloseHandle(threadSnapshot);

  snapshot.nameCounts.reserve(counts.size());
  for (const auto& [name, count] : counts) {
    snapshot.nameCounts.push_back(NamedThreadCount{name, count});
  }
  std::sort(snapshot.nameCounts.begin(), snapshot.nameCounts.end(),
            [](const NamedThreadCount& a, const NamedThreadCount& b) {
              if (a.count != b.count) {
                return a.count > b.count;
              }
              return a.name < b.name;
            });
#endif

  return snapshot;
}

QString sharedBackgroundThreadPoolDebugString()
{
  const auto snapshot = sharedBackgroundThreadPoolSnapshot();
  return QStringLiteral("%1 active=%2/%3 expiry=%4ms")
      .arg(snapshot.poolName.isEmpty() ? QStringLiteral("BackgroundPool") : snapshot.poolName)
      .arg(snapshot.activeThreadCount)
      .arg(snapshot.maxThreadCount)
      .arg(snapshot.expiryTimeoutMs);
}

QString currentProcessThreadDebugString()
{
  const auto snapshot = currentProcessThreadSnapshot();
  QStringList groups;
  const int maxGroups = 8;
  for (int i = 0; i < std::min<int>(maxGroups, static_cast<int>(snapshot.nameCounts.size())); ++i) {
    const auto& item = snapshot.nameCounts[static_cast<size_t>(i)];
    groups << QStringLiteral("%1 x%2").arg(item.name, QString::number(item.count));
  }
  return QStringLiteral("ProcessThreads total=%1 [%2]")
      .arg(snapshot.totalThreadCount)
      .arg(groups.join(QStringLiteral(", ")));
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
