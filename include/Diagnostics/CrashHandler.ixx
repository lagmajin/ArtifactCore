module;
#include <windows.h>
#include <QString>

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <atomic>

export module Diagnostics.CrashHandler;

export namespace ArtifactCore {

 class CrashHandler {
 public:
  static void install(const QString& crashDir = QString());
  static void uninstall();
  static bool isInstalled();

 private:
  static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
  static QString generateCrashReport(EXCEPTION_POINTERS* exceptionInfo);
  static void writeCrashReport(const QString& report, const QString& dumpPath);
  static QString captureStackTrace(CONTEXT* context, int maxFrames = 64);
  static QString getSystemInfo();
  static QString crashDirectory();

  static inline std::atomic<bool> installed_{false};
  static inline std::atomic<bool> handling_{false};
  static inline QString crashDir_;
 };

}
