function Write-Utf8Transcript {
  param(
    [Parameter(Mandatory = $true)]
    [scriptblock]$Command,

    [Parameter(Mandatory = $true)]
    [string]$OutPath,

    [string]$FailureMessage = "command failed"
  )

  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutPath) | Out-Null
  $encoding = [System.Text.UTF8Encoding]::new($false)
  $writer = [System.IO.StreamWriter]::new($OutPath, $false, $encoding)
  try {
    $global:LASTEXITCODE = 0
    & $Command 2>&1 | ForEach-Object { $writer.WriteLine([string]$_) }
    $exitCode = $LASTEXITCODE
  } finally {
    $writer.Dispose()
  }

  if ($exitCode -ne 0) {
    throw $FailureMessage
  }
}
