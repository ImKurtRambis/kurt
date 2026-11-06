# Plays the game in a 1600x900 window (starts at the desk menu, L_Menu).
# Usage: powershell -ExecutionPolicy Bypass -File Tools\Run.ps1 [-Map L_Scene]
param([string]$Map = "")
$Engine = "C:\Program Files\Epic Games\UE_5.8"
$Project = Join-Path (Split-Path $PSScriptRoot -Parent) "HomicideUnit.uproject"
$MapArg = if ($Map) { "/Game/Maps/$Map" } else { "" }
Start-Process -FilePath "$Engine\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"$Project`" $MapArg -game -windowed -ResX=1600 -ResY=900"
