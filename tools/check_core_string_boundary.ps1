$checks = @(
    @{ File = "src/Utils/UniString.cppm"; MustContain = "toUtf8()"; Name = "UniString UTF-8 conversion" },
    @{ File = "src/Utils/UniString.cppm"; MustNotContain = "std::string(toStdU16String().begin()"; Name = "UTF-16 byte-copy regression" },
    @{ File = "include/Utils/Text/Encoding.ixx"; MustContain = "isValidUtf8"; Name = "Core UTF-8 validation" },
    @{ File = "include/Utils/Text/Encoding.ixx"; MustContain = "firstInvalidUtf8Offset"; Name = "UTF-8 error offset" },
    @{ File = "include/Utils/Text/Encoding.ixx"; MustContain = "toUtf32Checked"; Name = "Core UTF-32 conversion" },
    @{ File = "include/Utils/Text/Encoding.ixx"; MustContain = "utf8CodepointCount"; Name = "Core code point count" },
    @{ File = "include/Utils/Text/Encoding.ixx"; MustContain = "detectBom"; Name = "Core BOM detection" },
    @{ File = "include/Utils/Text/Encoding.ixx"; MustContain = "stripBom"; Name = "Core BOM stripping" },
    @{ File = "include/Utils/Text/Encoding.ixx"; MustContain = "fromUtf8BomAware"; Name = "BOM-aware UTF-8 conversion" },
    @{ File = "include/Utils/Text/Encoding.ixx"; MustContain = "ARTIFACT_CORE_SOURCE_LOCATION"; Name = "Encoding source location" },
    @{ File = "include/Utils/Text/String.ixx"; MustContain = "splitView"; Name = "Core split view" },
    @{ File = "include/Utils/Text/String.ixx"; MustContain = "std::string join"; Name = "Core string join" },
    @{ File = "include/Utils/Text/String.ixx"; MustContain = "trimView"; Name = "Core trim view" },
    @{ File = "include/Utils/Text/String.ixx"; MustContain = "startsWith"; Name = "Core starts with" },
    @{ File = "include/Utils/Text/String.ixx"; MustContain = "endsWith"; Name = "Core ends with" },
    @{ File = "include/Utils/Text/Number.ixx"; MustContain = "parseInt64"; Name = "Core signed number parsing" },
    @{ File = "include/Utils/Text/Number.ixx"; MustContain = "parseUInt64"; Name = "Core unsigned number parsing" },
    @{ File = "include/Utils/Text/Number.ixx"; MustContain = "parseBool"; Name = "Core boolean parsing" },
    @{ File = "include/Utils/Text/Path.ixx"; MustContain = "normalizePathSeparators"; Name = "Core path normalization" },
    @{ File = "include/Utils/Text/Path.ixx"; MustContain = "isAbsolutePath"; Name = "Core absolute path check" },
    @{ File = "include/Utils/Text/Path.ixx"; MustContain = "hasParentTraversal"; Name = "Core traversal check" },
    @{ File = "include/Utils/Text/Path.ixx"; MustContain = "isSafeRelativePath"; Name = "Core safe relative path check" },
    @{ File = "src/Diagnostics/CoreDiagnostic.Test.cppm"; MustContain = "pathContractTest"; Name = "Path contract test" },
    @{ File = "include/Utils/Text/Number.ixx"; MustContain = "ARTIFACT_CORE_SOURCE_LOCATION"; Name = "Number source location" },
    @{ File = "docs/CORE_NATIVE_STRING_BOUNDARY_PLAN_2026-07-11.md"; MustContain = "UTF-8"; Name = "String boundary plan" }
)

$failures = @()
foreach ($check in $checks) {
    if (-not (Test-Path -LiteralPath $check.File)) {
        $failures += "missing file: $($check.File)"
        continue
    }
    $content = Get-Content -LiteralPath $check.File -Raw
    if ($check.MustContain -and $content -notlike "*$($check.MustContain)*") {
        $failures += "missing string contract: $($check.Name)"
    }
    if ($check.MustNotContain -and $content -like "*$($check.MustNotContain)*") {
        $failures += "regression detected: $($check.Name)"
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "Core string boundary OK ($($checks.Count) checks)."
