$contracts = @(
    @{ File = "include/Utils/Result.ixx"; Pattern = "struct ErrorContext"; Name = "ErrorContext" },
    @{ File = "include/Utils/Result.ixx"; Pattern = "file != nullptr"; Name = "Source location validity" },
    @{ File = "include/Utils/Result.ixx"; Pattern = "errorContext\(\)"; Name = "Result errorContext" },
    @{ File = "include/Diagnostics/CoreDiagnosticSnapshot.ixx"; Pattern = "struct DiagnosticSnapshot"; Name = "DiagnosticSnapshot" },
    @{ File = "include/Diagnostics/CoreDiagnosticSnapshot.ixx"; Pattern = "diagnosticSnapshotToJson"; Name = "Snapshot JSON" },
    @{ File = "include/Diagnostics/CoreDiagnosticSnapshot.ixx"; Pattern = "latestFailureFunction"; Name = "Failure source function" },
    @{ File = "include/Diagnostics/CoreDiagnosticRecorder.ixx"; Pattern = "class DiagnosticRecorder"; Name = "DiagnosticRecorder" },
    @{ File = "include/Diagnostics/CoreDiagnosticRecorder.ixx"; Pattern = "std::vector<DiagnosticEvent> since"; Name = "Recorder delta API" },
    @{ File = "include/Diagnostics/CoreDiagnosticRecorder.ixx"; Pattern = "snapshotSince"; Name = "Recorder delta snapshot" },
    @{ File = "include/Diagnostics/CoreCrashReportParser.ixx"; Pattern = "loadCrashReportSnapshot"; Name = "Crash Snapshot adapter" },
    @{ File = "include/Diagnostics/CoreCrashReportParser.ixx"; Pattern = "ARTIFACT_CORE_SOURCE_LOCATION"; Name = "Crash parser source location" },
    @{ File = "include/Diagnostics/CoreCrashReportParser.ixx"; Pattern = "Crash report path is empty"; Name = "Crash parser empty path" },
    @{ File = "include/Diagnostics/Logger.ixx"; Pattern = "flushDiagnostics"; Name = "Qt Logger adapter" },
    @{ File = "src/Diagnostics/Logger.cppm"; Pattern = "trace=%8"; Name = "Logger trace context" },
    @{ File = "src/Diagnostics/Logger.cppm"; Pattern = "function=%10"; Name = "Logger function context" },
    @{ File = "src/Codec/FFMpegVideoDecoder.cppm"; Pattern = "DiagnosticScope"; Name = "FFmpeg diagnostic connection" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "snapshotJsonContractTest"; Name = "Snapshot contract test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "crashReportParserContractTest"; Name = "Crash parser contract test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "recorderSequenceContractTest"; Name = "Recorder contract test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "recorderResultContractTest"; Name = "Result recording contract test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "recorderLegacyStatusContractTest"; Name = "Legacy status contract test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "recorderEmptyContextContractTest"; Name = "Empty context contract test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "recorderEnableContractTest"; Name = "Recorder enable contract test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "resultContextFactoryContractTest"; Name = "Result context factory test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "diagnosticScopeTraceContractTest"; Name = "Scope trace contract test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "recorderDeltaContractTest"; Name = "Delta snapshot contract test" },
    @{ File = "src/Codec/FFMpegVideoDecoder.cppm"; Pattern = '"decodeNextVideoFrame"'; Name = "Decoder frame scope" },
    @{ File = "src/Codec/FFMpegVideoDecoder.cppm"; Pattern = '"flush"'; Name = "Decoder flush scope" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "runAllCoreDiagnosticTests"; Name = "Diagnostic test runner" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "utf8EncodingContractTest"; Name = "UTF-8 contract test" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; Pattern = "bomDetectionContractTest"; Name = "BOM contract test" }
)

$failures = @()
foreach ($contract in $contracts) {
    if (-not (Test-Path -LiteralPath $contract.File)) {
        $failures += "missing file: $($contract.File) ($($contract.Name))"
        continue
    }
    if (-not (Select-String -LiteralPath $contract.File -Pattern $contract.Pattern -Quiet)) {
        $failures += "missing contract: $($contract.Name) in $($contract.File)"
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "Core diagnostics contract OK ($($contracts.Count) checks)."
