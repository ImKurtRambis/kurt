# Two-player test on one PC: a headless host on a level, and a headless client joining it over LAN.
# Usage: powershell -ExecutionPolicy Bypass -File Tools\TestSquad.ps1 [-Map L_Scene]
param([string]$Map = "L_Scene")
$Engine = "C:\Program Files\Epic Games\UE_5.8"
$Exe = "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path (Split-Path $PSScriptRoot -Parent) "HomicideUnit.uproject"
$HostLog = Join-Path $env:TEMP "HomicideUnit_host.log"
$ClientLog = Join-Path $env:TEMP "HomicideUnit_client.log"

$h = Start-Process -FilePath $Exe -PassThru -ArgumentList "`"$Project`" /Game/Maps/$Map?listen -game -nullrhi -unattended -nosound -nosplash -nosteam -abslog=`"$HostLog`""
Start-Sleep -Seconds 20
$c = Start-Process -FilePath $Exe -PassThru -ArgumentList "`"$Project`" 127.0.0.1 -game -nullrhi -unattended -nosound -nosplash -nosteam -abslog=`"$ClientLog`""
Start-Sleep -Seconds 40
foreach ($p in $h, $c) { if (!$p.HasExited) { Stop-Process -Id $p.Id -Force } }

"=== HOST";   Select-String -Path $HostLog   -Pattern "Join succeeded|Fatal|Assertion failed|Error: " | Select-Object -First 10 | ForEach-Object { $_.Line }
"=== CLIENT"; Select-String -Path $ClientLog -Pattern "Welcomed by server|Load map complete|Fatal|Assertion failed|Error: |NetworkFailure" | Select-Object -First 10 | ForEach-Object { $_.Line }
