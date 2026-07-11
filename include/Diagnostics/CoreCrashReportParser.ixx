module;
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

export module Core.Diagnostics.CrashReportParser;

import Core.Diagnostics.Snapshot;
import Utils.Result;

export namespace ArtifactCore {

struct CrashReportSummary {
  std::string timestamp;
  std::string exceptionCode;
  std::string exceptionType;
  std::string operation;
  std::string address;
  std::string exceptionAddress;
  std::string stackTrace;
  std::string systemInfo;
  bool parsed = false;
};

inline std::string_view crashReportValue(std::string_view line,
                                         std::string_view key)
{
  if (!line.starts_with(key)) return {};
  line.remove_prefix(key.size());
  while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
    line.remove_prefix(1);
  }
  return line;
}

inline CrashReportSummary parseCrashReport(std::string_view report)
{
  CrashReportSummary result;
  enum class Section { Header, Exception, Stack, System } section = Section::Header;
  std::size_t cursor = 0;
  while (cursor <= report.size()) {
    const std::size_t end = report.find('\n', cursor);
    std::string_view line = report.substr(
        cursor, end == std::string_view::npos ? report.size() - cursor : end - cursor);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

    if (line == "--- Exception ---") section = Section::Exception;
    else if (line == "--- Stack Trace ---") section = Section::Stack;
    else if (line == "--- System Info ---") section = Section::System;
    else if (const auto value = crashReportValue(line, "Timestamp:"); !value.empty()) {
      result.timestamp = std::string(value);
    } else if (section == Section::Exception) {
      if (const auto value = crashReportValue(line, "Code:"); !value.empty()) {
        result.exceptionCode = std::string(value);
      } else if (const auto value = crashReportValue(line, "Type:"); !value.empty()) {
        result.exceptionType = std::string(value);
      } else if (const auto value = crashReportValue(line, "Operation:"); !value.empty()) {
        result.operation = std::string(value);
      } else if (const auto value = crashReportValue(line, "Address:"); !value.empty()) {
        result.address = std::string(value);
      } else if (const auto value = crashReportValue(line, "Exception Address:"); !value.empty()) {
        result.exceptionAddress = std::string(value);
      }
    } else if (section == Section::Stack) {
      if (!line.empty()) {
        if (!result.stackTrace.empty()) result.stackTrace.push_back('\n');
        result.stackTrace.append(line);
      }
    } else if (section == Section::System) {
      if (!line.empty()) {
        if (!result.systemInfo.empty()) result.systemInfo.push_back('\n');
        result.systemInfo.append(line);
      }
    }

    if (end == std::string_view::npos) break;
    cursor = end + 1;
  }
  result.parsed = report.find("=== Artifact Crash Report ===") != std::string_view::npos;
  return result;
}

inline DiagnosticSnapshot crashReportToSnapshot(const CrashReportSummary& report)
{
  DiagnosticSnapshot snapshot;
  snapshot.component = "CrashHandler";
  snapshot.objectId = report.exceptionAddress;
  snapshot.state = report.exceptionType;
  snapshot.lastOperation = report.operation;
  DiagnosticEvent event = makeDiagnosticEvent(
      CoreDiagnosticSeverity::Fatal,
      report.exceptionCode.empty() ? "crash.unknown" :
                                     "crash." + report.exceptionCode,
      report.exceptionType.empty() ? "Unhandled exception" : report.exceptionType,
      "CrashHandler",
      report.operation.empty() ? "unhandledException" : report.operation,
      report.exceptionAddress);
  if (!report.stackTrace.empty()) {
    event.message += "\n" + report.stackTrace;
  }
  snapshot.addEvent(std::move(event));
  return snapshot;
}

inline Result<CrashReportSummary> parseCrashReportFile(const std::string& filePath)
{
  if (filePath.empty()) {
    return Result<CrashReportSummary>::fail(ErrorContext{
        .code = ErrorCode::InvalidArgument,
        .message = "Crash report path is empty",
        .operation = "parseCrashReportFile",
        .location = ARTIFACT_CORE_SOURCE_LOCATION});
  }
  std::ifstream file(filePath, std::ios::binary);
  if (!file) {
    return Result<CrashReportSummary>::fail(ErrorContext{
        .code = ErrorCode::NotFound,
        .message = "Crash report could not be opened",
        .operation = "parseCrashReportFile",
        .objectId = filePath,
        .location = ARTIFACT_CORE_SOURCE_LOCATION});
  }

  std::ostringstream contents;
  contents << file.rdbuf();
  const auto summary = parseCrashReport(contents.str());
  if (!summary.parsed) {
    return Result<CrashReportSummary>::fail(ErrorContext{
        .code = ErrorCode::InvalidArgument,
        .message = "Crash report format is not recognized",
        .operation = "parseCrashReportFile",
        .objectId = filePath,
        .location = ARTIFACT_CORE_SOURCE_LOCATION});
  }
  return Result<CrashReportSummary>::ok(summary);
}

inline Result<DiagnosticSnapshot> loadCrashReportSnapshot(const std::string& filePath)
{
  auto report = parseCrashReportFile(filePath);
  if (!report) {
    return Result<DiagnosticSnapshot>::fail(report.errorContext());
  }
  return Result<DiagnosticSnapshot>::ok(crashReportToSnapshot(report.value()));
}

} // namespace ArtifactCore
