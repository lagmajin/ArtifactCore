module;

#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp")
#include <QString>
#include <QStringView>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>
#include <QSysInfo>

module Diagnostics.CrashHandler;

import std;

namespace ArtifactCore {

void CrashHandler::install(const QString& crashDir)
{
 if (installed_.exchange(true)) return;

 crashDir_ = crashDir.isEmpty() ? crashDirectory() : crashDir;
 QDir dir(crashDir_);
 if (!dir.exists()) {
  dir.mkpath(QStringLiteral("."));
 }

 SetUnhandledExceptionFilter(unhandledExceptionFilter);
 qDebug() << "[CrashHandler] installed, crash dir:" << crashDir_;
}

void CrashHandler::uninstall()
{
 if (!installed_.exchange(false)) return;
 SetUnhandledExceptionFilter(nullptr);
 qDebug() << "[CrashHandler] uninstalled";
}

void CrashHandler::setCrashCallback(CrashCallback callback)
{
 crashCallback_ = std::move(callback);
}

bool CrashHandler::isInstalled()
{
 return installed_.load();
}

LONG WINAPI CrashHandler::unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
 if (handling_.exchange(true)) {
  return EXCEPTION_EXECUTE_HANDLER;
 }

 QString report = generateCrashReport(exceptionInfo);

 const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
  const QString dumpPath = QDir(crashDir_).filePath(
   QStringLiteral("crash_%1.log").arg(QStringView{timestamp}));

 writeCrashReport(report, dumpPath);

 qDebug() << "[CrashHandler] crash report written to:" << dumpPath;

 if (crashCallback_) {
  crashCallback_(dumpPath);
 }

 return EXCEPTION_EXECUTE_HANDLER;
}

QString CrashHandler::generateCrashReport(EXCEPTION_POINTERS* exceptionInfo)
{
 QString report;
 QTextStream ts(&report);

 ts << "=== Artifact Crash Report ===\n";
 ts << "Timestamp: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

 if (exceptionInfo && exceptionInfo->ExceptionRecord) {
  auto* rec = exceptionInfo->ExceptionRecord;
  ts << "--- Exception ---\n";
  ts << "Code: 0x" << QString::number(rec->ExceptionCode, 16).toUpper() << "\n";

  switch (rec->ExceptionCode) {
   case EXCEPTION_ACCESS_VIOLATION:
    ts << "Type: Access Violation\n";
    if (rec->NumberParameters >= 2) {
     ts << "Operation: " << (rec->ExceptionInformation[0] == 0 ? "Read" :
                             rec->ExceptionInformation[0] == 1 ? "Write" : "Execute") << "\n";
     ts << "Address: 0x" << QString::number(rec->ExceptionInformation[1], 16).toUpper() << "\n";
    }
    break;
   case EXCEPTION_STACK_OVERFLOW:
    ts << "Type: Stack Overflow\n";
    break;
   case EXCEPTION_ILLEGAL_INSTRUCTION:
    ts << "Type: Illegal Instruction\n";
    break;
   case EXCEPTION_INT_DIVIDE_BY_ZERO:
    ts << "Type: Integer Divide by Zero\n";
    break;
   case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    ts << "Type: Float Divide by Zero\n";
    break;
   default:
    ts << "Type: Unknown\n";
    break;
  }

  ts << "Flags: 0x" << QString::number(rec->ExceptionFlags, 16) << "\n";
  ts << "Exception Address: 0x" << QString::number(
   reinterpret_cast<uintptr_t>(rec->ExceptionAddress), 16).toUpper() << "\n";

  if (exceptionInfo->ContextRecord) {
   auto* ctx = exceptionInfo->ContextRecord;
#ifdef _M_X64
   ts << "RIP: 0x" << QString::number(ctx->Rip, 16).toUpper() << "\n";
   ts << "RSP: 0x" << QString::number(ctx->Rsp, 16).toUpper() << "\n";
   ts << "RBP: 0x" << QString::number(ctx->Rbp, 16).toUpper() << "\n";
#elif defined(_M_IX86)
   ts << "EIP: 0x" << QString::number(ctx->Eip, 16).toUpper() << "\n";
   ts << "ESP: 0x" << QString::number(ctx->Esp, 16).toUpper() << "\n";
   ts << "EBP: 0x" << QString::number(ctx->Ebp, 16).toUpper() << "\n";
#endif
  }
 }

 ts << "\n--- Stack Trace ---\n";
 if (exceptionInfo && exceptionInfo->ContextRecord) {
  ts << captureStackTrace(exceptionInfo->ContextRecord);
 } else {
  ts << "(no context available)\n";
 }

 ts << "\n--- System Info ---\n";
 ts << getSystemInfo();

 return report;
}

QString CrashHandler::captureStackTrace(CONTEXT* context, int maxFrames)
{
 QString result;
 QTextStream ts(&result);

 HANDLE process = GetCurrentProcess();
 HANDLE thread = GetCurrentThread();

 SymInitialize(process, nullptr, TRUE);
 SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);

 CONTEXT ctx = *context;

 STACKFRAME64 frame{};
#ifdef _M_X64
 frame.AddrPC.Offset = ctx.Rip;
 frame.AddrFrame.Offset = ctx.Rbp;
 frame.AddrStack.Offset = ctx.Rsp;
 frame.AddrPC.Mode = AddrModeFlat;
 frame.AddrFrame.Mode = AddrModeFlat;
 frame.AddrStack.Mode = AddrModeFlat;
 DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_IX86)
 frame.AddrPC.Offset = ctx.Eip;
 frame.AddrFrame.Offset = ctx.Ebp;
 frame.AddrStack.Offset = ctx.Esp;
 frame.AddrPC.Mode = AddrModeFlat;
 frame.AddrFrame.Mode = AddrModeFlat;
 frame.AddrStack.Mode = AddrModeFlat;
 DWORD machineType = IMAGE_FILE_MACHINE_I386;
#endif

 char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
 auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
 symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
 symbol->MaxNameLen = MAX_SYM_NAME;

 IMAGEHLP_LINE64 line{};
 line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

 for (int i = 0; i < maxFrames; ++i) {
  BOOL ok = StackWalk64(machineType, process, thread, &frame, &ctx,
                        nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr);
  if (!ok || frame.AddrPC.Offset == 0) break;

  DWORD64 displacement = 0;
  DWORD lineDisplacement = 0;
  QString funcName;
  QString fileName;
  int lineNumber = 0;

  if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
   funcName = QString::fromUtf8(symbol->Name);
  } else {
   funcName = QStringLiteral("???");
  }

  if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line)) {
   fileName = QString::fromUtf8(line.FileName);
   lineNumber = static_cast<int>(line.LineNumber);
  }

  ts << QString::number(i).rightJustified(3, QChar(' '))
     << "  0x" << QString::number(frame.AddrPC.Offset, 16).toUpper().rightJustified(16, QChar('0'))
     << "  " << funcName;

  if (!fileName.isEmpty()) {
   ts << "  (" << fileName << ":" << lineNumber << ")";
  }
  ts << "\n";
 }

 SymCleanup(process);
 return result;
}

QString CrashHandler::getSystemInfo()
{
 QString info;
 QTextStream ts(&info);

 ts << "OS: " << QSysInfo::prettyProductName() << "\n";
 ts << "Kernel: " << QSysInfo::kernelType() << " " << QSysInfo::kernelVersion() << "\n";
 ts << "Architecture: " << QSysInfo::currentCpuArchitecture() << "\n";

 MEMORYSTATUSEX mem{};
 mem.dwLength = sizeof(mem);
 if (GlobalMemoryStatusEx(&mem)) {
  ts << "Physical Memory Total: " << (mem.ullTotalPhys / (1024 * 1024)) << " MB\n";
  ts << "Physical Memory Avail: " << (mem.ullAvailPhys / (1024 * 1024)) << " MB\n";
  ts << "Memory Load: " << mem.dwMemoryLoad << "%\n";
 }

 ts << "Process ID: " << GetCurrentProcessId() << "\n";
 ts << "Thread ID: " << GetCurrentThreadId() << "\n";

 return info;
}

void CrashHandler::writeCrashReport(const QString& report, const QString& dumpPath)
{
 QFile file(dumpPath);
 if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
  file.write(report.toUtf8());
  file.close();
 }
}

QString CrashHandler::crashDirectory()
{
 const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
 return QDir(appData).filePath(QStringLiteral("CrashDumps"));
}

}
