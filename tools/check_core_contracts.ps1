$ErrorActionPreference = "Stop"

$checks = @(
    "check_core_native_boundary.ps1",
    "check_core_module_hygiene.ps1",
    "check_core_diagnostics_contract.ps1",
    "check_core_string_boundary.ps1"
)

foreach ($check in $checks) {
    & (Join-Path $PSScriptRoot $check)
    if (-not $?) {
        throw "Core contract failed: $check"
    }
}

Write-Output "Core contract gate OK ($($checks.Count) checks)."
