param(
  [string]$Out = "reports/HOST_PROFILE.json",
  [string]$ProfileLabel = "",
  [string]$IntendedHardware = "0",
  [string]$ApprovedHost = "0"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$OutPath = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $Root $Out }
$IntendedHardwareBool = @("1", "true", "yes", "on") -contains $IntendedHardware.ToLowerInvariant()
$ApprovedHostBool = @("1", "true", "yes", "on") -contains $ApprovedHost.ToLowerInvariant()

function Get-CommandPath {
  param([string]$Name)
  try {
    $cmd = Get-Command $Name -ErrorAction Stop | Select-Object -First 1
    return [string]$cmd.Source
  } catch {
    return ""
  }
}

function Get-OperatingSystemInfo {
  try {
    return Get-CimInstance Win32_OperatingSystem -OperationTimeoutSec 5 -ErrorAction Stop
  } catch {
    return $null
  }
}

function Get-ProcessorInfo {
  try {
    return @(Get-CimInstance Win32_Processor -OperationTimeoutSec 5 -ErrorAction Stop)
  } catch {
    return @()
  }
}

$os = Get-OperatingSystemInfo
$cpus = Get-ProcessorInfo
$cpu = if ($cpus.Count -gt 0) { $cpus | Select-Object -First 1 } else { $null }
$computer = $null
try {
  $computer = Get-CimInstance Win32_ComputerSystem -OperationTimeoutSec 5 -ErrorAction Stop
} catch {
  $computer = $null
}
$physicalCores = if ($cpus.Count -gt 0) { @($cpus | Measure-Object -Property NumberOfCores -Sum).Sum } else { 0 }
$logicalCores = if ($cpus.Count -gt 0) { @($cpus | Measure-Object -Property NumberOfLogicalProcessors -Sum).Sum } else { [Environment]::ProcessorCount }

$profile = [ordered]@{
  generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
  platform = "windows"
  profile_label = $ProfileLabel
  intended_hardware = $IntendedHardwareBool
  approved_host = $ApprovedHostBool
  hostname = $env:COMPUTERNAME
  os = [ordered]@{
    caption = if ($os) { $os.Caption } else { [System.Environment]::OSVersion.VersionString }
    version = if ($os) { $os.Version } else { [System.Environment]::OSVersion.Version.ToString() }
    build_number = if ($os) { $os.BuildNumber } else { "" }
  }
  cpu = [ordered]@{
    name = if ($cpu) { $cpu.Name } else { $env:PROCESSOR_IDENTIFIER }
    manufacturer = if ($cpu) { $cpu.Manufacturer } else { "" }
    logical_cores = $logicalCores
    physical_cores = $physicalCores
  }
  memory = [ordered]@{
    total_physical_bytes = if ($computer) { [int64]$computer.TotalPhysicalMemory } else { 0 }
  }
  toolchain = [ordered]@{
    cmake = (Get-CommandPath "cmake")
    clang = (Get-CommandPath "clang")
    clangxx = (Get-CommandPath "clang++")
    msvc = (Get-CommandPath "cl")
    gcc = (Get-CommandPath "g++")
  }
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutPath) | Out-Null
$profile | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $OutPath
Write-Output "host_profile=$OutPath"
