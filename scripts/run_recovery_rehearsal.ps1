param(
  [string]$Cli = ".\build-release\graphenedb_cli.exe",
  [string]$WorkDir = "",
  [string]$Out = "reports/RECOVERY_REHEARSAL_OUTPUT.txt"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Cli)) {
  throw "graphenedb_cli not found at $Cli. Build it first, for example: cmake --build build-release --target graphenedb_cli"
}

if ([string]::IsNullOrWhiteSpace($WorkDir)) {
  $WorkDir = Join-Path ([System.IO.Path]::GetTempPath()) "graphenedb_recovery_rehearsal"
}

Remove-Item -Recurse -Force $WorkDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $WorkDir | Out-Null
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null

$Db = Join-Path $WorkDir "source-db"
$Backup = Join-Path $WorkDir "restored-db"
$Tsv = Join-Path $WorkDir "incident.tsv"
$Sig = [UInt64]((1 -shl 3) -bor (1 -shl 19))

$Rows = @(
  "root-restore`tRecovery rehearsal root cause: stale cache after deploy`t0.90,0.05,0.03,0.02`t$Sig`t9101`troot",
  "symptom-restore`tRecovery rehearsal symptom: checkout timeout`t0.05,0.90,0.03,0.02`t$Sig`t9101`tsymptom",
  "impact-restore`tRecovery rehearsal impact: conversion loss`t0.05,0.10,0.90,0.02`t$Sig`t9101`timpact"
)
[System.IO.File]::WriteAllLines($Tsv, $Rows, [System.Text.UTF8Encoding]::new($false))

$lines = New-Object System.Collections.Generic.List[string]
function Add-Line([string]$Line) {
  $lines.Add($Line) | Out-Null
  Write-Output $Line
}

Add-Line "recovery_rehearsal=true"
Add-Line "source_db=$Db"
Add-Line "restored_db=$Backup"

$ImportOut = & $Cli extract-tsv $Db 4 recovery-pack $Tsv --wal-rotate-bytes 2048
if ($LASTEXITCODE -ne 0) { throw "extract-tsv failed" }
if (($ImportOut -join "`n") -notmatch "inserted_nodes=3") { throw "expected inserted_nodes=3" }
Add-Line "import=pass"

$Validate = (& $Cli validate $Db 4 --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $Validate.validation -ne "ok") { throw "source validation failed" }
Add-Line "source_validate=pass"

$Compact = (& $Cli compact $Db 4 --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $Compact.compacted -ne $true) { throw "source compact failed" }
Add-Line "source_compact=pass"

$BackupResult = (& $Cli backup $Db 4 $Backup --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $BackupResult.backup_verified -ne $true) { throw "backup verification failed" }
Add-Line "backup_verified=pass"

$RestoredInspect = (& $Cli inspect $Backup 4 --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw "restored inspect failed" }
if ($RestoredInspect.nodes_visible -ne 3 -or $RestoredInspect.edges_visible -ne 2) { throw "unexpected restored counts" }
Add-Line "restored_nodes=$($RestoredInspect.nodes_visible)"
Add-Line "restored_edges=$($RestoredInspect.edges_visible)"

$RestoredValidate = (& $Cli validate $Backup 4 --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $RestoredValidate.validation -ne "ok") { throw "restored validation failed" }
Add-Line "restored_validate=pass"

$Search = & $Cli search $Backup 4 "0.05,0.90,0.03,0.02" $Sig
if ($LASTEXITCODE -ne 0) { throw "restored search failed" }
if (($Search -join "`n") -notmatch "target=") { throw "restored search did not return a target" }
Add-Line "restored_search=pass"

$Neighbors = & $Cli neighbors $Backup 4 0 2
if ($LASTEXITCODE -ne 0) { throw "restored neighbors failed" }
if (($Neighbors -join "`n") -notmatch "1") { throw "restored neighbors did not include expected neighbor" }
Add-Line "restored_neighbors=pass"

Add-Line "recovery_rehearsal_passed=true"
$lines | Set-Content -Encoding UTF8 $Out
Write-Output "recovery_rehearsal_output=$Out"
