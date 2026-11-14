# Packages a standalone Windows build into Packaged\Windows (HomicideUnit.exe runs on a PC without Unreal installed;
# a PC that has never installed a game may first need Redist\vc_redist.x64.exe, Microsoft's Visual C++ runtime).
#
# The local AI (AI\ + Tools\VoiceServer.*) is copied beside the game so suspects think and speak as they do in the
# editor. Without it the game still plays: suspects answer with written lines. -NoAI makes that small build.
#
# Usage: powershell -ExecutionPolicy Bypass -File Tools\Package.ps1 [-Config Development] [-NoAI]
#   Shipping (default) has no console and writes no log; Development keeps both, for chasing a packaged-only bug.
param([ValidateSet("Shipping", "Development")][string]$Config = "Shipping", [switch]$NoAI)
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
$Engine = "C:\Program Files\Epic Games\UE_5.8"
$Out = Join-Path $Root "Packaged"

& "$Engine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\HomicideUnit.uproject" -platform=Win64 `
    "-clientconfig=$Config" -build -cook -stage -pak -iostore -compressed -prereqs -package -archive `
    "-archivedirectory=$Out" -nop4 -utf8output -unattended
if ($LASTEXITCODE -ne 0) { throw "Packaging failed (RunUAT exit code $LASTEXITCODE) - the errors are above." }

$Game = Join-Path $Out "Windows"
$Proj = Join-Path $Game "HomicideUnit"   # FPaths::ProjectDir() in the packaged game

# Steam: a build started outside the Steam client needs steam_appid.txt beside the exe (the editor writes it for itself,
# Shipping doesn't). This is Valve's test app 480 until the game has its own App ID; Steam supplies it for store builds.
$AppId = (Select-String -Path "$Root\Config\DefaultEngine.ini" -Pattern "^SteamDevAppId=(\d+)").Matches[0].Groups[1].Value
Set-Content -Path (Join-Path $Proj "Binaries\Win64\steam_appid.txt") -Value $AppId -Encoding ascii -NoNewline

if ($NoAI) {
    foreach ($d in "AI", "Tools") { if (Test-Path "$Proj\$d") { Remove-Item "$Proj\$d" -Recurse -Force } }
} elseif (Test-Path "$Root\AI\llama\llama-server.exe") {
    # /MIR only copies what changed, so packaging again is quick; logs and the unused Chatterbox install stay behind
    robocopy "$Root\AI" "$Proj\AI" /MIR /XF *.err *.out *.log /XD chatterbox chatterbox-dl /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copying the AI folder failed (robocopy exit code $LASTEXITCODE)." }
    New-Item -ItemType Directory -Force "$Proj\Tools" | Out-Null
    Copy-Item "$Root\Tools\VoiceServer.bat", "$Root\Tools\VoiceServer.py" "$Proj\Tools" -Force
} else {
    Write-Warning "AI\llama isn't installed here, so the build has no local AI (suspects use written lines)."
}

# UE 5.8 no longer stages its prerequisites installer, so ship Microsoft's Visual C++ runtime installer beside the game
New-Item -ItemType Directory -Force "$Game\Redist" | Out-Null
Copy-Item "$Engine\Engine\Extras\Redist\en-us\vc_redist.x64.exe" "$Game\Redist" -Force

# never ship anyone's paid-voice key
if (Test-Path "$Proj\ElevenLabsKey.txt") { Remove-Item "$Proj\ElevenLabsKey.txt" -Force }

$GB = (Get-ChildItem $Game -Recurse -File | Measure-Object Length -Sum).Sum / 1GB
"Packaged $Config build: $Game\HomicideUnit.exe ({0:N1} GB)" -f $GB
