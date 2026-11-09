# Copyright (C) 2026 Tencent. All rights reserved.

[CmdletBinding()]
param(
  [ValidateSet("win")]
  [string]$Platform = "win",

  [ValidateSet("x64", "arm64")]
  [string]$Arch = $(if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64" -or
      $env:PROCESSOR_ARCHITEW6432 -eq "ARM64") { "arm64" } else { "x64" }),

  [ValidateRange(1, 256)]
  [int]$Jobs = 0,

  [switch]$SkipSync,

  [switch]$Clean
)

$ErrorActionPreference = "Stop"
$MsysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" }
$Bash = Join-Path $MsysRoot "usr\bin\bash.exe"
$Cygpath = Join-Path $MsysRoot "usr\bin\cygpath.exe"

if (-not (Test-Path $Bash -PathType Leaf)) {
  throw "MSYS2 bash was not found at $Bash. Set MSYS2_ROOT to your MSYS2 installation."
}
if (-not (Test-Path $Cygpath -PathType Leaf)) {
  throw "MSYS2 cygpath was not found at $Cygpath."
}

$env:MSYSTEM = if ($Arch -eq "arm64") { "CLANGARM64" } else { "MINGW64" }
$env:CHERE_INVOKING = "1"
$ScriptPath = (& $Cygpath -u (Join-Path $PSScriptRoot "build.sh")).Trim()
if ($LASTEXITCODE -ne 0 -or -not $ScriptPath) {
  throw "Failed to convert the build script path to an MSYS2 path."
}

$BuildArguments = @("--login", $ScriptPath, "--platform", $Platform, "--arch", $Arch)
if ($Jobs -gt 0) {
  $BuildArguments += @("--jobs", $Jobs)
}
if ($SkipSync) {
  $BuildArguments += "--skip-sync"
}
if ($Clean) {
  $BuildArguments += "--clean"
}

& $Bash @BuildArguments
exit $LASTEXITCODE
