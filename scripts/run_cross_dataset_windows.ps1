param(
    [ValidateSet('offline', 'public', 'all')]
    [string]$Mode = 'offline'
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Build = Join-Path $Root 'build\cross_dataset_portable'
$Reports = Join-Path $Root 'reports\cross_dataset\portable'
$Evidence = Join-Path $Reports 'evidence'
$Python = if ($env:PYTHON) { $env:PYTHON } else { 'python' }
$BabiPerTask = if ($env:BABI_PER_TASK) { $env:BABI_PER_TASK } else { '200' }
$Hotpot = if ($env:HOTPOT) { $env:HOTPOT } else { '500' }
$FeverPerLabel = if ($env:FEVER_PER_LABEL) { $env:FEVER_PER_LABEL } else { '200' }
$Seed = if ($env:CROSS_DATASET_SEED) { $env:CROSS_DATASET_SEED } else { '20260729' }

New-Item -ItemType Directory -Force -Path $Build, $Reports, $Evidence | Out-Null

$Exe = Join-Path $Build 'bench_cross_dataset_epistemic.exe'
$Gxx = Get-Command 'g++' -ErrorAction SilentlyContinue
$Cl = Get-Command 'cl.exe' -ErrorAction SilentlyContinue

Write-Host '[1/6] Compiling actual FiberBundleBuilder and LyapunovCritic'
if ($Gxx) {
    & $Gxx.Source -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror `
        "-I$Root\include" `
        "$Root\bench\bench_cross_dataset_epistemic.cpp" `
        "$Root\src\fiber_bundle.cpp" `
        "$Root\src\stability_critic.cpp" `
        -o $Exe
    $Compiler = $Gxx.Source
} elseif ($Cl) {
    & $Cl.Source /nologo /std:c++20 /EHsc /O2 /W4 /WX `
        "/I$Root\include" `
        "$Root\bench\bench_cross_dataset_epistemic.cpp" `
        "$Root\src\fiber_bundle.cpp" `
        "$Root\src\stability_critic.cpp" `
        "/Fe:$Exe"
    $Compiler = $Cl.Source
} else {
    throw 'Neither g++ nor cl.exe is available. Install MinGW-w64 or Visual Studio Build Tools.'
}

function Invoke-Suite([string]$Label, [string]$InputPath) {
    $Prefix = Join-Path $Reports $Label
    & $Exe $InputPath "${Prefix}_results.csv"
    & $Python "$Root\benchmarks\cross_dataset\summarise_cross_dataset.py" `
        --input "${Prefix}_results.csv" `
        --json-output "${Prefix}_summary.json" `
        --markdown-output "${Prefix}_summary.md" `
        --enforce
}

Write-Host '[2/6] Enforcing committed source-isolated regression'
Invoke-Suite 'source_isolated' "$Root\benchmarks\cross_dataset\source_isolated_actual.tsv"

$PublicInput = Join-Path $Build 'examples.tsv'
if ($Mode -in @('public', 'all')) {
    Write-Host '[3/6] Downloading and normalising frozen public benchmark records'
    & $Python "$Root\benchmarks\cross_dataset\prepare_cross_dataset.py" `
        --output $PublicInput `
        --babi-per-task $BabiPerTask `
        --hotpot $Hotpot `
        --fever-per-label $FeverPerLabel `
        --seed $Seed

    Write-Host '[4/6] Enforcing public benchmark gates'
    Invoke-Suite 'public' $PublicInput
} else {
    Write-Host "[3/6] Public download skipped (mode=$Mode)"
    Write-Host "[4/6] Public benchmark skipped (mode=$Mode)"
}

$Inputs = @(
    'bench/bench_cross_dataset_epistemic.cpp',
    'benchmarks/cross_dataset/source_isolated_actual.tsv',
    'benchmarks/cross_dataset/summarise_cross_dataset.py',
    'include/graphene/fiber_bundle.hpp',
    'include/graphene/stability_critic.hpp',
    'src/fiber_bundle.cpp',
    'src/stability_critic.cpp',
    'reports/cross_dataset/portable/source_isolated_results.csv',
    'reports/cross_dataset/portable/source_isolated_summary.json'
)
if (Test-Path $PublicInput) {
    $Inputs += @(
        'build/cross_dataset_portable/examples.tsv',
        'build/cross_dataset_portable/examples.manifest.json',
        'reports/cross_dataset/portable/public_results.csv',
        'reports/cross_dataset/portable/public_summary.json'
    )
}

Write-Host '[5/6] Capturing environment and evidence'
$Capture = @(
    "$Root\scripts\capture_cross_dataset_environment.py",
    '--repo', $Root,
    '--output', "$Evidence\environment.json",
    '--mode', $Mode,
    '--compiler', $Compiler
)
foreach ($Input in $Inputs) { $Capture += @('--input', $Input) }
& $Python @Capture

try { git -C $Root rev-parse HEAD | Set-Content -Encoding ascii "$Evidence\source_commit.txt" }
catch { 'unknown' | Set-Content -Encoding ascii "$Evidence\source_commit.txt" }
Copy-Item "$Reports\source_isolated_results.csv", "$Reports\source_isolated_summary.json", "$Reports\source_isolated_summary.md" -Destination $Evidence -Force
if (Test-Path "$Reports\public_results.csv") {
    Copy-Item "$Reports\public_results.csv", "$Reports\public_summary.json", "$Reports\public_summary.md", $PublicInput, "$PublicInput.manifest.json" -Destination $Evidence -Force
}

$ChecksumLines = Get-ChildItem -File -Recurse $Evidence | Where-Object { $_.Name -ne 'checksums.sha256' } | Sort-Object FullName | ForEach-Object {
    $Hash = (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLowerInvariant()
    "$Hash  $($_.FullName.Substring($Evidence.Length + 1).Replace('\', '/'))"
}
$ChecksumLines | Set-Content -Encoding ascii "$Evidence\checksums.sha256"

Write-Host '[6/6] Packaging evidence'
$Commit = (Get-Content "$Evidence\source_commit.txt" -Raw).Trim()
if ($Commit.Length -gt 12) { $Commit = $Commit.Substring(0, 12) }
$Archive = Join-Path $Reports "cross-dataset-$Mode-$Commit.zip"
Compress-Archive -Path "$Evidence\*" -DestinationPath $Archive -Force

Write-Host 'portable_cross_dataset_status=PASS'
Write-Host "mode=$Mode"
Write-Host "evidence=$Evidence"
Write-Host "archive=$Archive"
