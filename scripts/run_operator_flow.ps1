param(
  [string]$Cli = ".\build-release\graphenedb_cli.exe",
  [string]$WorkDir = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Cli)) {
  throw "graphenedb_cli not found at $Cli. Build it first, for example: cmake --build build-release --target graphenedb_cli"
}

if ([string]::IsNullOrWhiteSpace($WorkDir)) {
  $WorkDir = Join-Path ([System.IO.Path]::GetTempPath()) "graphenedb_operator_flow"
}

Remove-Item -Recurse -Force $WorkDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $WorkDir | Out-Null

$Db = Join-Path $WorkDir "db"
$Backup = Join-Path $WorkDir "backup"
$Tsv = Join-Path $WorkDir "extraction.tsv"
$Sig = [UInt64]((1 -shl 2) -bor (1 -shl 18))

$Rows = @(
  "root-1`tCheckout root cause: stale edge cache after migration`t0.90,0.10,0.05,0.01`t$Sig`t9001`troot",
  "symptom-1`tCheckout timeout symptom observed by customers`t0.10,0.90,0.05,0.01`t$Sig`t9001`tsymptom",
  "impact-1`tPayment failures impacted conversion`t0.10,0.20,0.90,0.01`t$Sig`t9001`timpact"
)
[System.IO.File]::WriteAllLines($Tsv, $Rows, [System.Text.UTF8Encoding]::new($false))

& $Cli extract-tsv $Db 4 operator-pack $Tsv --wal-rotate-bytes 2048 | Tee-Object -Variable ImportOut
if ($LASTEXITCODE -ne 0) { throw "extract-tsv failed" }
if (($ImportOut -join "`n") -notmatch "inserted_nodes=3") { throw "expected inserted_nodes=3" }

$Inspect = (& $Cli inspect $Db 4 --vector-index kdtree --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw "inspect failed" }
if ($Inspect.nodes_visible -ne 3 -or $Inspect.edges_visible -ne 2) { throw "unexpected inspect counts" }
if ($Inspect.vector_index -ne "kdtree") { throw "expected kdtree vector index" }

$Validate = (& $Cli validate $Db 4 --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw "validate failed" }
if ($Validate.validation -ne "ok") { throw "validation not ok" }

$Compact = (& $Cli compact $Db 4 --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw "compact failed" }
if ($Compact.compacted -ne $true) { throw "compact JSON did not report true" }

$PostCompact = (& $Cli inspect $Db 4 --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw "post-compact inspect failed" }
if ($PostCompact.wal_bytes -ne 0) { throw "expected compacted WAL to be empty" }

$BackupResult = (& $Cli backup $Db 4 $Backup --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw "backup failed" }
if ($BackupResult.backup_verified -ne $true) { throw "backup was not verified" }

$Search = & $Cli search $Db 4 "0.10,0.90,0.05,0.01" $Sig
if ($LASTEXITCODE -ne 0) { throw "search failed" }
if (($Search -join "`n") -notmatch "target=") { throw "search did not return a target" }

Write-Output "operator_flow_passed=true"
Write-Output "db=$Db"
Write-Output "backup=$Backup"
