param(
  [Parameter(Mandatory=$true)][string]$Package,
  [Parameter(Mandatory=$true)][string]$InstallDir,
  [string]$Out = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$PackagePath = if ([System.IO.Path]::IsPathRooted($Package)) { $Package } else { Join-Path $Root $Package }
$InstallPath = if ([System.IO.Path]::IsPathRooted($InstallDir)) { $InstallDir } else { Join-Path $Root $InstallDir }
if ([string]::IsNullOrWhiteSpace($Out)) {
  $Out = "$PackagePath.manifest.json"
}
$OutPath = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $Root $Out }

if (-not (Test-Path $PackagePath)) { throw "package not found: $PackagePath" }
if (-not (Test-Path $InstallPath)) { throw "install dir not found: $InstallPath" }

$packageHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PackagePath).Hash.ToLowerInvariant()
$InstallFullPath = [System.IO.Path]::GetFullPath($InstallPath).TrimEnd('\', '/')
$files = Get-ChildItem -LiteralPath $InstallPath -Recurse -File | Sort-Object FullName | ForEach-Object {
  $full = [System.IO.Path]::GetFullPath($_.FullName)
  $relative = $full.Substring($InstallFullPath.Length).TrimStart('\', '/').Replace('\', '/')
  [ordered]@{
    path = $relative
    size = $_.Length
    sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
  }
}

$manifest = [ordered]@{
  name = "GrapheneDB"
  version = "0.5.0"
  package = [System.IO.Path]::GetFileName($PackagePath)
  package_size = (Get-Item -LiteralPath $PackagePath).Length
  package_sha256 = $packageHash
  generated_utc = [DateTime]::UtcNow.ToString("o")
  install_prefix = [System.IO.Path]::GetFileName($InstallPath)
  files = @($files)
}

$manifest | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $OutPath
Set-Content -Encoding ASCII -Path "$PackagePath.sha256" -Value "$packageHash  $([System.IO.Path]::GetFileName($PackagePath))"
Write-Output "release_manifest=$OutPath"
Write-Output "package_sha256=$packageHash"
