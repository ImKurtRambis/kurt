# The interview test bench: plays the scripted interviews in Tools\InterviewBench\questions.json through the suspects'
# local brain (the game's own prompts and rules), then scores them. Run it after any change to the model or its prompt
# and compare with the baseline. Takes about 6 minutes for the default 2 cases (3 to interview, 3 to score). Close the game first (the brain's
# port is shared).
#
# Usage: powershell -ExecutionPolicy Bypass -File Tools\InterviewBench.ps1 [-Cases 2] [-SaveBaseline] [-NoJudge]
#   -SaveBaseline  make this run the one later runs are compared with (do it when you accept a change)
#   -NoJudge       rules only (quick, rougher)
param([int]$Cases = 2, [switch]$SaveBaseline, [switch]$NoJudge)
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
$Engine = "C:\Program Files\Epic Games\UE_5.8"
$Out = Join-Path $Root ("Saved\InterviewBench\" + (Get-Date -Format "yyyy-MM-dd_HHmm"))
New-Item -ItemType Directory -Force $Out | Out-Null
$Log = Join-Path $Out "game.log"

"Interviewing ($Cases cases) - the game runs without a window; log: $Log"
$Game = Start-Process "$Engine\Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -ArgumentList (
    "`"$Root\HomicideUnit.uproject`" /Game/Maps/L_Menu -game -nullrhi -nosound -unattended -nosplash -HUNoVoice " +
    "-HUInterviewBench -HUBenchCases=$Cases -HUBenchOut=`"$Out`" -abslog=`"$Log`"")
$Game.WaitForExit()
if (-not (Test-Path "$Out\turns.jsonl")) { throw "The bench wrote nothing - see $Log" }
$Meta = Get-Content "$Out\meta.json" -Raw | ConvertFrom-Json
if ($Meta.error) { Write-Warning "The bench stopped early: $($Meta.error)" }

"Scoring..."
$Py = @("-I", "$Root\Tools\InterviewBench\score.py", $Out, "--root", $Root)
if ($SaveBaseline) { $Py += "--save-baseline" }
if ($NoJudge) { $Py += "--no-judge" }
& "$Root\AI\python\python.exe" @Py
if ($LASTEXITCODE -ne 0) { throw "Scoring failed" }
