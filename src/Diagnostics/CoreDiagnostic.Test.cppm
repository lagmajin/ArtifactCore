module;
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>
#include <span>

export module Core.Diagnostics.Test;

import Core.Diagnostics.CrashReportParser;
import Core.Diagnostics.Recorder;
import Core.Diagnostics.Snapshot;
import Diagnostics.Logger;
import Diagnostics.CrashHandler;
import Utils.Text.Encoding;
import Utils.Text.Number;
import Utils.Text.Path;
import Utils.Text.String;

namespace ArtifactCore::DiagnosticsTest {

bool snapshotJsonContractTest()
{
  DiagnosticSnapshot snapshot;
  DiagnosticEvent later = makeDiagnosticEvent(
      CoreDiagnosticSeverity::Error,
      "test.later",
      "later failure",
      "Test",
      "later",
      "object-1");
  later.sequence = 2;
  DiagnosticEvent earlier = makeDiagnosticEvent(
      CoreDiagnosticSeverity::Error,
      "test.earlier",
      "earlier failure",
      "Test",
      "earlier",
      "object-1");
  earlier.sequence = 1;
  snapshot.addEvent(later);
  snapshot.addEvent(earlier);
  snapshot.component = "Test";
  DiagnosticEvent current = makeDiagnosticEvent(
      CoreDiagnosticSeverity::Error,
      "test.failure",
      "failure message",
      "Test",
      "run",
      "object-1");
  current.sequence = 3;
  snapshot.addEvent(current);
  DiagnosticSnapshot unsorted;
  unsorted.recentEvents = {current, earlier, later};
  const std::string json = diagnosticSnapshotToJson(snapshot);
  return snapshot.hasErrors() && snapshot.latestFailure() != nullptr &&
         snapshot.latestFailure()->code == "test.later" &&
         unsorted.latestFailure() != nullptr &&
         unsorted.latestFailure()->code == "test.later" &&
         json.find("\"hasErrors\":true") != std::string::npos &&
         json.find("test.failure") != std::string::npos &&
         json.find("latestFailureFunction") != std::string::npos;
}

bool crashReportParserContractTest()
{
  constexpr std::string_view report =
      "=== Artifact Crash Report ===\n"
      "Timestamp: 2026-07-11T12:00:00\n"
      "--- Exception ---\n"
      "Code: 0xC0000005\n"
      "Type: Access Violation\n"
      "Operation: Read\n"
      "Address: 0x1234\n"
      "Exception Address: 0x5678\n"
      "--- Stack Trace ---\n"
      "frame 0\n"
      "--- System Info ---\n"
      "OS: Windows\n";
  const auto summary = parseCrashReport(report);
  const auto snapshot = crashReportToSnapshot(summary);
  return summary.parsed && summary.exceptionCode == "0xC0000005" &&
         snapshot.hasErrors() && snapshot.latestFailure() != nullptr;
}

bool recorderSequenceContractTest()
{
  auto& recorder = DiagnosticRecorder::instance();
  const bool wasEnabled = recorder.isEnabled();
  recorder.setEnabled(true);
  recorder.clear();
  recorder.record(CoreDiagnosticSeverity::Info,
                  "test.first", "first", "Test", "sequence");
  recorder.record(CoreDiagnosticSeverity::Warning,
                  "test.second", "second", "Test", "sequence");
  const auto events = recorder.snapshot();
  const bool valid = events.size() == 2 &&
                     events[0].sequence < events[1].sequence;
  recorder.clear();
  recorder.setEnabled(wasEnabled);
  return valid;
}

bool recorderResultContractTest()
{
  auto& recorder = DiagnosticRecorder::instance();
  recorder.clear();
  const auto failure = Result<int>::fail(ErrorContext{
      .code = ErrorCode::NotFound,
      .message = "asset missing",
      .operation = "asset.open",
      .objectId = "asset-7"});
  const bool recorded = recorder.recordResult(failure, "AssetLoader", 12);
  const auto events = recorder.snapshot();
  return recorded && events.size() == 1 &&
         events.front().code == "core.not_found" &&
         events.front().component == "AssetLoader" &&
         events.front().frameIndex == 12;
}

bool recorderLegacyStatusContractTest()
{
  auto& recorder = DiagnosticRecorder::instance();
  recorder.clear();
  const Status legacyStatus{false, ErrorCode::Busy, {}};
  recorder.recordStatus(legacyStatus, "LegacyQueue");
  const auto events = recorder.snapshot();
  return events.size() == 1 &&
         events.front().code == "core.busy" &&
         events.front().message == "busy";
}

bool recorderEmptyContextContractTest()
{
  auto& recorder = DiagnosticRecorder::instance();
  recorder.clear();
  recorder.recordError(ErrorContext{}, "UnknownSource");
  const auto event = recorder.latest();
  return event.has_value() && event->code == "core.failed";
}

bool recorderEnableContractTest()
{
  auto& recorder = DiagnosticRecorder::instance();
  recorder.clear();
  recorder.setEnabled(false);
  recorder.record(CoreDiagnosticSeverity::Info, "disabled", "ignored", "Test", "run");
  const bool suppressed = recorder.size() == 0;
  recorder.setEnabled(true);
  recorder.record(CoreDiagnosticSeverity::Info, "enabled", "kept", "Test", "run");
  return suppressed && recorder.size() == 1;
}

bool resultContextFactoryContractTest()
{
  const auto failure = Result<int>::fail(
      ErrorCode::Busy, "queue is busy", "queue.push", "queue-1",
      sourceLocation(__FILE__, __func__, __LINE__));
  return !failure && failure.errorContext().code == ErrorCode::Busy &&
         failure.errorContext().operation == "queue.push" &&
         failure.errorContext().objectId == "queue-1" &&
         failure.errorContext().location.line > 0;
}

bool diagnosticScopeTraceContractTest()
{
  auto& recorder = DiagnosticRecorder::instance();
  recorder.clear();
  {
    DiagnosticScope scope("Decoder", "decode", "frame-1");
    scope.finish(true);
  }
  const auto events = recorder.snapshot();
  return events.size() == 1 && events.front().traceId != 0;
}

bool recorderDeltaContractTest()
{
  auto& recorder = DiagnosticRecorder::instance();
  recorder.clear();
  recorder.setMaxEvents(3);
  recorder.record(CoreDiagnosticSeverity::Info, "one", "1", "Delta", "run");
  const auto cursor = recorder.latestSequence();
  recorder.record(CoreDiagnosticSeverity::Info, "two", "2", "Delta", "run");
  recorder.record(CoreDiagnosticSeverity::Info, "other", "x", "Other", "run");
  recorder.record(CoreDiagnosticSeverity::Info, "three", "3", "Delta", "run");
  const auto delta = recorder.snapshotSince(cursor - 1, "Delta");
  recorder.setMaxEvents(256);
  return delta.eventsTruncated && delta.recentEvents.size() == 2 &&
         delta.lastSequence > delta.firstSequence &&
         delta.lastOperation == "run" &&
         delta.recentEvents.front().component == "Delta" &&
         delta.recentEvents.back().component == "Delta";
}

bool utf8EncodingContractTest()
{
  constexpr std::string_view valid = "\xE3\x81\x82\xF0\x9F\x98\x80";
  constexpr std::string_view invalidOverlong = "\xC0\xAF";
  const auto codepoints = toUtf32Checked(valid);
  const auto count = utf8CodepointCount(valid);
  const auto invalid = fromUtf8Checked(invalidOverlong);
  const auto invalidBomAware = fromUtf8BomAware("\xEF\xBB\xBF\xC0\xAF");
  const auto bomAware = fromUtf8BomAware("\xEF\xBB\xBFhello");
  return isValidUtf8(valid) && !isValidUtf8(invalidOverlong) &&
         codepoints && codepoints.value().size() == 2 &&
         count && count.value() == 2 && bomAware &&
         bomAware.value() == "hello" && !invalid &&
         invalid.errorContext().objectId == "0" && !invalidBomAware &&
         invalidBomAware.errorContext().operation == "encoding.fromUtf8BomAware" &&
         invalidBomAware.errorContext().objectId == "0" &&
         invalidBomAware.errorContext().location.line > 0;
}

bool bomDetectionContractTest()
{
  constexpr std::string_view utf8Bom = "\xEF\xBB\xBFtext";
  constexpr std::string_view utf16LeBom = "\xFF\xFEtext";
  constexpr std::string_view utf32LeBom("\xFF\xFE\x00\x00text", 8);
  constexpr std::string_view utf32BeBom("\x00\x00\xFE\xFFtext", 8);
  return detectBom(utf8Bom) == ByteOrderMark::Utf8 &&
         detectBom(utf16LeBom) == ByteOrderMark::Utf16LittleEndian &&
         detectBom(utf32LeBom) == ByteOrderMark::Utf32LittleEndian &&
         detectBom(utf32BeBom) == ByteOrderMark::Utf32BigEndian &&
         detectBom("plain") == ByteOrderMark::None &&
         stripBom(utf8Bom) == "text";
}

bool stringViewContractTest()
{
  const auto parts = splitView("a,,c", ',');
  const std::span<const std::string_view> view(parts.data(), parts.size());
  return parts.size() == 3 && parts[1].empty() &&
         join(view, '|') == "a||c" &&
         trimView(" \tvalue\r\n") == "value" &&
         startsWith("artifact.core", "artifact") &&
         endsWith("artifact.core", "core");
}

bool numericParsingContractTest()
{
  const auto signedValue = parseInt64("-42", "frame.index");
  const auto unsignedValue = parseUInt64("184", "frame.count");
  const auto enabled = parseBool("true", "feature.enabled");
  const auto invalid = parseInt64("42ms", "timestamp");
  const auto invalidBool = parseBool("yes", "feature.enabled");
  return signedValue && signedValue.value() == -42 &&
         unsignedValue && unsignedValue.value() == 184 && !invalid &&
         enabled && enabled.value() && !invalidBool &&
         invalid.errorContext().operation == "number.parseInt64" &&
         invalid.errorContext().objectId == "timestamp" &&
         invalid.errorContext().location.hasValue();
}

bool pathContractTest()
{
  const auto normalized = normalizePathSeparators("C:\\\\work\\\\asset.mov", "asset.path");
  const auto unc = normalizePathSeparators("\\\\server\\\\share\\\\asset.mov", "asset.unc");
  constexpr std::string_view nulPath("bad\0path", 8);
  const auto invalid = normalizePathSeparators(nulPath, "asset.path");
  return normalized && normalized.value() == "C:/work/asset.mov" &&
         unc && unc.value() == "//server/share/asset.mov" &&
         isAbsolutePath(normalized.value()) && !invalid &&
         hasParentTraversal("cache/../asset.mov") &&
         hasParentTraversal("cache\\..\\asset.mov") &&
         !hasParentTraversal("cache/asset.mov") &&
         isSafeRelativePath("cache/asset.mov") &&
         !isSafeRelativePath("/cache/asset.mov") &&
         !isSafeRelativePath("cache/../asset.mov") &&
         !isSafeRelativePath("cache\\..\\asset.mov") &&
         invalid.errorContext().operation == "path.normalizeSeparators";
}

bool crashHandlerIngestContractTest()
{
  auto& recorder = DiagnosticRecorder::instance();
  const bool wasEnabled = recorder.isEnabled();
  recorder.setEnabled(true);
  recorder.clear();

  const std::string dir = std::filesystem::temp_directory_path().string() +
                          "/artifact_crash_ingest_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  constexpr std::string_view report =
      "=== Artifact Crash Report ===\n"
      "Timestamp: 2026-07-11T12:00:00\n"
      "--- Exception ---\n"
      "Code: 0xC0000005\n"
      "Type: Access Violation\n"
      "Operation: Read\n"
      "Address: 0x1234\n"
      "Exception Address: 0x5678\n"
      "--- Stack Trace ---\n"
      "frame 0\n"
      "--- System Info ---\n"
      "OS: Windows\n";
  const std::string reportPath = dir + "/crash_20260711_120000.log";
  {
    std::ofstream out(reportPath, std::ios::binary);
    out << report;
  }

  const auto pending = CrashHandler::pendingReportPaths(QString::fromStdString(dir));
  CrashHandler::ingestPendingReports(QString::fromStdString(dir));

  const auto events = recorder.snapshot();
  recorder.clear();
  recorder.setEnabled(wasEnabled);
  std::filesystem::remove_all(dir);

  bool foundFatal = false;
  for (const auto& event : events) {
    if (event.severity == CoreDiagnosticSeverity::Fatal &&
        event.code == "crash.0xC0000005") {
      foundFatal = true;
    }
  }
  return pending.size() == 1 && foundFatal;
}

bool loggerReverseAdapterContractTest()
{
  auto& recorder = DiagnosticRecorder::instance();
  recorder.clear();
  const bool wasEnabled = recorder.isEnabled();
  recorder.setEnabled(true);

  Logger::instance()->recordDiagnostic(LogLevel::Error, QStringLiteral("disk write failed"));
  const auto events = recorder.snapshot();
  recorder.clear();
  recorder.setEnabled(wasEnabled);

  return events.size() == 1 &&
         events.front().severity == CoreDiagnosticSeverity::Error &&
         events.front().code == "qt.log.error" &&
         events.front().component == "QtLogger" &&
         events.front().message == "disk write failed";
}

bool runAllCoreDiagnosticTests()
{
  return snapshotJsonContractTest() &&
         crashReportParserContractTest() &&
         recorderSequenceContractTest() &&
         recorderResultContractTest() &&
         recorderLegacyStatusContractTest() &&
         recorderEmptyContextContractTest() &&
         recorderEnableContractTest() &&
         resultContextFactoryContractTest() &&
         diagnosticScopeTraceContractTest() &&
         recorderDeltaContractTest() &&
         utf8EncodingContractTest() &&
         bomDetectionContractTest() &&
         stringViewContractTest() &&
         numericParsingContractTest() &&
         pathContractTest() &&
         loggerReverseAdapterContractTest() &&
         crashHandlerIngestContractTest();
}

} // namespace ArtifactCore::DiagnosticsTest
