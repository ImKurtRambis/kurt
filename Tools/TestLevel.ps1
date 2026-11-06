# Loads a level headless (no window), lets it run, and reports errors from the log.
# Usage: powershell -ExecutionPolicy Bypass -File Tools\TestLevel.ps1 -Map L_Scene [-Seconds 35]
param([string]$Map = "L_Scene", [int]$Seconds = 35)
$Engine = "C:\Program Files\Epic Games\UE_5.8"
$Project = Join-Path (Split-Path $PSScriptRoot -Parent) "HomicideUnit.uproject"
$Log = Join-Path $env:TEMP "HomicideUnit_test_$Map.log"

$p = Start-Process -FilePath "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -PassThru -ArgumentList `
	"`"$Project`" /Game/Maps/$Map -game -nullrhi -unattended -nosound -nosplash -nosteam -abslog=`"$Log`""
Start-Sleep -Seconds $Seconds
if (!$p.HasExited) { Stop-Process -Id $p.Id -Force }

$loaded = Select-String -Path $Log -Pattern "Load map complete /Game/Maps/$Map" -Quiet
$errors = Select-String -Path $Log -Pattern "Fatal error|Assertion failed|Error: |Accessed None|Unhandled Exception"
if ($loaded) { "LOADED: $Map" } else { "DID NOT LOAD: $Map" }
if ($errors) { "ERRORS:"; $errors | Select-Object -First 20 | ForEach-Object { $_.Line } } else { "No errors." }
"Full log: $Log"
