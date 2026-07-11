$paths = @(
    "include/Utils/Result.ixx",
    "include/Utils/Text/Encoding.ixx",
    "include/Utils/Text/String.ixx",
    "include/Utils/Text/Number.ixx",
    "include/Utils/Text/Path.ixx",
    "include/Diagnostics/CoreDiagnosticSnapshot.ixx",
    "include/Diagnostics/CoreDiagnosticRecorder.ixx",
    "include/Diagnostics/CoreCrashReportParser.ixx"
)

$violations = @()
foreach ($path in $paths) {
    $moduleSeen = $false
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $path) {
        $lineNumber++
        if ($line -match '^\s*(export\s+)?module\s+[^;]+;') {
            $moduleSeen = $true
            continue
        }
        if ($moduleSeen -and $line -match '^\s*#include\s+') {
            $violations += "${path}:${lineNumber}: include after module declaration"
        }
    }
}

if ($violations.Count -gt 0) {
    $violations | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "Core module hygiene OK ($($paths.Count) files checked)."
