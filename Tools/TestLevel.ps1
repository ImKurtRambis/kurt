# Loads a level headless, reports runtime errors, and optionally checks character models and interactions.
# Usage: Tools/TestLevel.ps1 -Map L_Scene [-Seconds 35] [-Characters]
param([string]$Map = 'L_Scene', [int]$Seconds = 35, [switch]$Characters)
$ErrorActionPreference = 'Stop'
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$Project = Join-Path (Split-Path $PSScriptRoot -Parent) 'HomicideUnit.uproject'
$Prefix = if ($Characters) { 'characters' } else { 'test' }
$Log = Join-Path $env:TEMP "HomicideUnit_${Prefix}_$Map.log"
$CharacterArgs = if ($Characters) { ' -ExecCmds=HU.TestCharacters -HUCharacterTest -HUCrime=homicide' } else { '' }

$p = Start-Process -WindowStyle Hidden -FilePath "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -PassThru -ArgumentList `
	"`"$Project`" /Game/Maps/$Map -game -nullrhi -unattended -nosound -nosplash -nosteam -nocrashdialog -abslog=`"$Log`"$CharacterArgs"
$timer = [Diagnostics.Stopwatch]::StartNew()
while (!$p.HasExited -and $timer.Elapsed.TotalSeconds -lt $Seconds) { Start-Sleep -Milliseconds 250; $p.Refresh() }
if (!$p.HasExited) { Stop-Process -Id $p.Id -Force }

$loaded = Select-String -LiteralPath $Log -Pattern "Load map complete /Game/Maps/$Map" -Quiet
$errors = @(Select-String -LiteralPath $Log -Pattern 'Fatal error|Assertion failed|Error: |Accessed None|Unhandled Exception')
$checks = @(Select-String -LiteralPath $Log -Pattern 'HUCharacterTest: PASSED|HUCharacterTest: FAILED')
if ($loaded) { "LOADED: $Map" } else { "DID NOT LOAD: $Map" }
if ($errors.Count) { $errors | Select-Object -First 20 | ForEach-Object { $_.Line } } else { 'No errors.' }
if ($Characters) { $checks | ForEach-Object { $_.Line } }
"Full log: $Log"
if (!$loaded -or $errors.Count -or ($Characters -and !($checks | Where-Object { $_.Line -match 'HUCharacterTest: PASSED' }))) { exit 1 }
exit 0
