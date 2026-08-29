module;

#include <windows.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <utility>

module Diagnostics.CrashHandler;

namespace ArtifactCore {

namespace {

QString normalizeCrashDirectory(const QString& crashDir)
{
  if (!crashDir.isEmpty()) {
    return crashDir;
  }

  if (auto* app = QCoreApplication::instance()) {
    const QString appDir = QFileInfo(app->applicationFilePath()).absolutePath();
    return QDir(appDir).filePath(QStringLiteral("crash_reports"));
  }

  return QDir::current().filePath(QStringLiteral("crash_reports"));
}

QString timestampedCrashPath(const QString& crashDir)
{
  const QString stamp =
      QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
  return QDir(crashDir).filePath(QStringLiteral("crash-%1.txt").arg(stamp));
}

QString exceptionCodeString(DWORD code)
{
  return QStringLiteral("0x%1")
      .arg(static_cast<qulonglong>(code), 8, 16, QLatin1Char('0'))
      .toUpper();
}

} // namespace

void CrashHandler::install(const QString& crashDir)
{
  crashDir_ = normalizeCrashDirectory(crashDir);
  QDir().mkpath(crashDir_);
  SetUnhandledExceptionFilter(&CrashHandler::unhandledExceptionFilter);
  installed_.store(true, std::memory_order_release);
}

void CrashHandler::uninstall()
{
  SetUnhandledExceptionFilter(nullptr);
  installed_.store(false, std::memory_order_release);
}

bool CrashHandler::isInstalled()
{
  return installed_.load(std::memory_order_acquire);
}

void CrashHandler::setCrashCallback(CrashCallback callback)
{
  crashCallback_ = std::move(callback);
}

CrashHandler::CrashReportPaths CrashHandler::pendingReportPaths(const QString& crashDir)
{
  CrashReportPaths result;
  const QString dirPath = normalizeCrashDirectory(crashDir);
  QDir dir(dirPath);
  if (!dir.exists()) {
    return result;
  }

  QFileInfoList files =
      dir.entryInfoList({QStringLiteral("crash-*.txt")}, QDir::Files, QDir::Time);
  result.reserve(files.size());
  for (const QFileInfo& fileInfo : files) {
    result.push_back(fileInfo.absoluteFilePath());
  }
  std::reverse(result.begin(), result.end());
  return result;
}

void CrashHandler::ingestPendingReports(const QString& crashDir)
{
  const auto reports = pendingReportPaths(crashDir);
  for (const QString& reportPath : reports) {
    if (crashCallback_) {
      crashCallback_(reportPath);
    }
  }
}

LONG WINAPI CrashHandler::unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
  if (handling_.exchange(true, std::memory_order_acq_rel)) {
    return EXCEPTION_EXECUTE_HANDLER;
  }

  const QString dumpPath = timestampedCrashPath(crashDirectory());
  const QString report = generateCrashReport(exceptionInfo);
  writeCrashReport(report, dumpPath);

  if (crashCallback_) {
    crashCallback_(dumpPath);
  }

  handling_.store(false, std::memory_order_release);
  return EXCEPTION_EXECUTE_HANDLER;
}

QString CrashHandler::generateCrashReport(EXCEPTION_POINTERS* exceptionInfo)
{
  QString report;
  QTextStream stream(&report);
  stream << "ArtifactStudio crash report\n";
  stream << "Timestamp (UTC): "
         << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "\n";
  stream << "System:\n" << getSystemInfo() << "\n";

  if (exceptionInfo && exceptionInfo->ExceptionRecord) {
    stream << "ExceptionCode: "
           << exceptionCodeString(exceptionInfo->ExceptionRecord->ExceptionCode)
           << "\n";
    stream << "ExceptionAddress: 0x"
           << QString::number(
                  reinterpret_cast<qulonglong>(
                      exceptionInfo->ExceptionRecord->ExceptionAddress),
                  16)
                  .toUpper()
           << "\n";
  } else {
    stream << "ExceptionCode: unknown\n";
  }

  stream << "\nStackTrace:\n"
         << captureStackTrace(exceptionInfo ? exceptionInfo->ContextRecord : nullptr)
         << "\n";
  return report;
}

void CrashHandler::writeCrashReport(const QString& report, const QString& dumpPath)
{
  QDir().mkpath(QFileInfo(dumpPath).absolutePath());
  QFile file(dumpPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    return;
  }

  QTextStream stream(&file);
  stream << report;
}

QString CrashHandler::captureStackTrace(CONTEXT* /*context*/, int maxFrames)
{
  const int frameLimit = std::clamp(maxFrames, 1, 128);
  void* frames[128] = {};
  const USHORT captured = CaptureStackBackTrace(
      0, static_cast<DWORD>(frameLimit), frames, nullptr);

  if (captured == 0) {
    return QStringLiteral("Stack trace capture returned no frames.");
  }

  QString trace;
  QTextStream stream(&trace);
  for (USHORT index = 0; index < captured; ++index) {
    stream << "#" << index << " 0x"
           << QString::number(
                  reinterpret_cast<qulonglong>(frames[index]), 16)
                  .toUpper()
           << "\n";
  }
  return trace;
}

QString CrashHandler::getSystemInfo()
{
  QString info;
  QTextStream stream(&info);
  stream << "ApplicationPid=" << QCoreApplication::applicationPid() << "\n";
  stream << "ApplicationPath="
         << (QCoreApplication::instance()
                 ? QCoreApplication::applicationFilePath()
                 : QStringLiteral("<none>"))
         << "\n";
  return info;
}

QString CrashHandler::crashDirectory()
{
  return normalizeCrashDirectory(crashDir_);
}

} // namespace ArtifactCore
