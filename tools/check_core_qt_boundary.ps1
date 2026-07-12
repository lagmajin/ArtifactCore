<#
.SYNOPSIS
  Core-native 公開 module の Qt 依存検査。

.DESCRIPTION
  Core-native として扱う公開 module（include/ 配下）を再帰走査し、
  module 宣言以降の purview に Qt の include/import/型が混入していないかを検査する。
  Global Module Fragment（module; 〜 export module X; の間）の Qt 依存は、
  Qt 境界 Adapter として正当なので許容する。

  検査対象は $coreNativeModules に明示列挙する。新しく Core-native 化した
  公開 module はこのリストへ追加することで、境界違反が自動検出されるようになる。
#>

$ErrorActionPreference = "Stop"

# Core-native として扱う公開 module（Qt から独立しているべき）。
# 新規に Core-native 化した module はここへ追加する。
$coreNativeModules = @(
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

foreach ($relativePath in $coreNativeModules) {
    if (-not (Test-Path -LiteralPath $relativePath)) {
        $violations += "missing: $relativePath"
        continue
    }

    $lineNumber = 0
    $inPurview = $false
    foreach ($line in Get-Content -LiteralPath $relativePath) {
        $lineNumber++

        # module 宣言を見つけたら Global Module Fragment を抜けて purview に入る。
        if ($line -match '^\s*(export\s+)?module\s+[^;]+;') {
            $inPurview = $true
            continue
        }

        if (-not $inPurview) {
            continue
        }

        if ($line -match '#include\s*[<"]Q[A-Za-z]' -or
            $line -match '^\s*import\s+(Qt|QtCore|QtGui|QtWidgets)' -or
            $line -match '\bQ[A-Z][A-Za-z0-9_]*\b') {
            $violations += "${relativePath}:${lineNumber}: Qt dependency in purview: $($line.Trim())"
        }
    }
}

if ($violations.Count -gt 0) {
    $violations | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "Core Qt boundary OK ($($coreNativeModules.Count) modules checked)."
