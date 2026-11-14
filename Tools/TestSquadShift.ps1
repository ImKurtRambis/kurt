# A whole squad shift, two players, on one PC, over a network that behaves like two homes: lag, jitter and lost packets.
# Two headless copies of the game (the editor build, no Steam: the local-network path) play it by themselves with
# -HUSquadRun (HUSquadRun.h has the script): host a squad, join it by its code, roll call, the briefing, the scene
# (bag, interview, cuff, van), the precinct (book, submit). Each copy logs what it checked and what its case looks like
# whenever it leaves a level; this compares the two copies stage by stage - in a squad they must agree.
# Each copy gets its own Saved folder, so your own save and settings aren't touched.
#
# Usage: powershell -ExecutionPolicy Bypass -File Tools\TestSquadShift.ps1 [-Lag 60] [-Jitter 20] [-Loss 1] [-Minutes 15] [-Game <exe>]
#   Lag: ms added to every packet each way (60 each way is a ~120 ms round trip, a long way across a country)
#   Jitter: up to this many ms more, at random;  Loss: % of packets dropped
#   Game: a packaged Development build instead of the editor build - the folder Package.ps1 made, or its exe
#         (Shipping builds leave the script out)
param([int]$Lag = 60, [int]$Jitter = 20, [int]$Loss = 1, [int]$Minutes = 15, [string]$Game = "")
$Engine = "C:\Program Files\Epic Games\UE_5.8"
$Exe = "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Root = Split-Path $PSScriptRoot -Parent
$Project = "`"$(Join-Path $Root "HomicideUnit.uproject")`""
$SavedRoot = $Root
if ($Game)
{
    # the game itself, not the launcher beside it (that one hands over and exits)
    if (Test-Path $Game -PathType Container) { $Game = Join-Path $Game "HomicideUnit\Binaries\Win64\HomicideUnit.exe" }
    if (-not (Test-Path $Game)) { throw "No packaged Development game at $Game" }
    $Exe = (Resolve-Path $Game).Path
    $Project = ""
    $SavedRoot = Split-Path (Split-Path (Split-Path $Exe -Parent) -Parent) -Parent   # ...\HomicideUnit, where its Saved goes
}
$Dir = Join-Path $env:TEMP "HUSquadRun"
if (Test-Path $Dir) { Remove-Item $Dir -Recurse -Force }
New-Item -ItemType Directory -Force $Dir | Out-Null
$HostLog = Join-Path $Dir "host.log"
$PartnerLog = Join-Path $Dir "partner.log"

$Net = "-PktLag=$Lag -PktLagVariance=$Jitter -PktLoss=$Loss"
$Common = "$Project /Game/Maps/L_Menu -game -nullrhi -unattended -nosound -nosplash -nosteam -HUNoBrain -HUNoVoice -HUSquadRunDir=`"$Dir`" $Net"
"Game: $Exe"
"Network: +$Lag ms each way (+0..$Jitter ms jitter), $Loss% packets lost. Logs: $Dir"
$h = Start-Process -FilePath $Exe -PassThru -ArgumentList "$Common -HUSquadRun=host -saveddirsuffix=SquadHost -abslog=`"$HostLog`""
Start-Sleep -Seconds 5
$p = Start-Process -FilePath $Exe -PassThru -ArgumentList "$Common -HUSquadRun=partner -saveddirsuffix=SquadPartner -abslog=`"$PartnerLog`""

$Deadline = (Get-Date).AddMinutes($Minutes)
while ((Get-Date) -lt $Deadline -and -not ($h.HasExited -and $p.HasExited)) { Start-Sleep -Seconds 5 }
$TimedOut = -not ($h.HasExited -and $p.HasExited)
foreach ($x in $h, $p) { if (-not $x.HasExited) { Stop-Process -Id $x.Id -Force } }
Start-Sleep -Seconds 2
foreach ($s in "SquadHost", "SquadPartner") { $d = Join-Path $SavedRoot "Saved_$s"; if (Test-Path $d) { Remove-Item $d -Recurse -Force -ErrorAction SilentlyContinue } }

function Lines($Log, $Pattern) { if (Test-Path $Log) { Select-String -Path $Log -Pattern $Pattern | ForEach-Object { $_.Line } } }
$Fail = 0
foreach ($Who in @(@("HOST", $HostLog), @("PARTNER", $PartnerLog)))
{
    "`n=== $($Who[0])"
    Lines $Who[1] "HUSquadRun (host|partner) \[" | ForEach-Object { ($_ -split "HUSquadRun ", 2)[1] }
    Lines $Who[1] "HUSquadRun CHECK" | ForEach-Object { $l = ($_ -split "HUSquadRun CHECK ", 2)[1]; if ($l -like "FAIL*") { $Fail++ }; "  CHECK $l" }
    Lines $Who[1] "Fatal error|Assertion failed|Unhandled Exception|NetworkFailure|TravelFailure" | Select-Object -First 5 | ForEach-Object { $Fail++; "  !! $_" }
}

# the digests, stage by stage: the same case on both PCs?
function Digests($Log) { $o = [ordered]@{}; Lines $Log "HUSquadRun DIGEST" | ForEach-Object { $d = ($_ -split "HUSquadRun DIGEST ", 2)[1]; $k = ($d -split " \| ", 2)[0]; $o[$k] = ($d -split " \| ", 2)[1] }; $o }
$HD = Digests $HostLog; $PD = Digests $PartnerLog
"`n=== THE TWO PCS' CASES"
foreach ($k in $HD.Keys)
{
    if (-not $PD.Contains($k)) { "  $k : the partner has no digest here"; $Fail++; continue }
    if ($HD[$k] -eq $PD[$k]) { "  $k : same  ($($HD[$k].Substring(0, [Math]::Min(150, $HD[$k].Length)))...)"; continue }
    $Fail++
    "  $k : DIFFERENT"
    $a = $HD[$k] -split " \| "; $b = $PD[$k] -split " \| "
    for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count); $i++) { if ($a[$i] -ne $b[$i]) { "      host:    $($a[$i])"; "      partner: $($b[$i])" } }
}
foreach ($k in $PD.Keys) { if (-not $HD.Contains($k)) { "  $k : only the partner has this digest"; $Fail++ } }

if ($TimedOut) { "`nTIMED OUT after $Minutes minutes"; $Fail++ }
if ($Fail -eq 0) { "`nPASS: a whole shift over a laggy network, and both PCs agree on the case." } else { "`nFAIL: $Fail problem(s) above." }
exit $Fail
