param(
  [string]$BuildDir = "build-release",
  [string]$Out = "reports/RC_REAL_KOSH_ADAPTER_OUTPUT.txt"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Utf8Transcript = Join-Path $Root "scripts/write_utf8_transcript.ps1"
. $Utf8Transcript
$Exe = Join-Path (Join-Path $Root $BuildDir) "graphenedb_rc_real_kosh_adapter_tests.exe"
$OutPath = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $Root $Out }

cmake --build (Join-Path $Root $BuildDir) --target graphenedb_rc_real_kosh_adapter_tests --config Release
if ($LASTEXITCODE -ne 0) { throw "build failed" }

New-Item -ItemType Directory -Force (Split-Path $OutPath) | Out-Null
Write-Utf8Transcript -OutPath $OutPath -FailureMessage "kosh adapter gate failed" -Command { & $Exe }
