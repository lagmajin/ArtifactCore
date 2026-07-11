param(
    [string[]]$Path = @(
        "include/Utils/Result.ixx",
        "include/Diagnostics/CoreDiagnosticSnapshot.ixx",
        "include/Diagnostics/CoreDiagnosticRecorder.ixx",
        "include/Diagnostics/CoreCrashReportParser.ixx",
        "include/Utils/Text/Encoding.ixx",
        "include/Utils/Text/String.ixx",
        "include/Utils/Text/Number.ixx",
        "include/Utils/Text/Path.ixx"
    )
)

$violations = @()

foreach ($relativePath in $Path) {
    if (-not (Test-Path -LiteralPath $relativePath)) {
        $violations += "missing: $relativePath"
        continue
    }

    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $relativePath) {
        $lineNumber++
        if ($line -match '#include\s*[<"]Q[A-Za-z]' -or
            $line -match '^\s*import\s+(Qt|QtCore|QtGui|QtWidgets)' -or
            $line -match '\bQ[A-Z][A-Za-z0-9_]*\b') {
            $violations += "${relativePath}:${lineNumber}: Qt dependency: $($line.Trim())"
        }
    }
}

if ($violations.Count -gt 0) {
    $violations | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "Core-native boundary OK ($($Path.Count) files checked)."
