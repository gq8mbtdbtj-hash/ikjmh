# Windows 一键依赖注入（等价 Linux patchelf --add-needed）
#
# 用法:
#   .\inject_memprobe.ps1 -Target C:\path\app.exe
#   .\inject_memprobe.ps1 -Target app.exe -Dll .\build\hooks\tray_memprobe.dll
#   .\inject_memprobe.ps1 -Target app.exe -Restore

param(
  [Parameter(Mandatory = $true)][string]$Target,
  [string]$Dll = "",
  [switch]$Restore
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Py = Join-Path $ScriptDir "inject_memprobe_pe.py"

if (-not (Get-Command python -ErrorAction SilentlyContinue) -and
    -not (Get-Command python3 -ErrorAction SilentlyContinue)) {
  Write-Error "Python 3 required. Install python.org or `winget install Python.Python.3.12`"
}

$Python = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "python3" }

$argsList = @($Py)
if ($Restore) {
  $argsList += @("--restore", $Target)
} else {
  $argsList += @($Target)
  if ($Dll -ne "") { $argsList += $Dll }
}

& $Python @argsList
exit $LASTEXITCODE
