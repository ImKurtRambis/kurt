# Builds the game (HomicideUnitEditor, Win64 Development). Closes a running copy of the game/editor first.
# Usage: powershell -ExecutionPolicy Bypass -File Tools\Build.ps1
$Engine = "C:\Program Files\Epic Games\UE_5.8"
$Project = Join-Path (Split-Path $PSScriptRoot -Parent) "HomicideUnit.uproject"

Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

& "$Engine\Engine\Build\BatchFiles\Build.bat" HomicideUnitEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReload |
	Select-String -Pattern "error|warning C|Result:" | ForEach-Object { $_.Line }
exit $LASTEXITCODE
