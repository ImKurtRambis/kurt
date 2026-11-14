# Packages a standalone Windows build into Packaged\Windows. HomicideUnit.exe runs on a PC with nothing installed but
# Windows 10/11 and a graphics driver: no Unreal, no Python, no Visual C++ runtime (its DLLs are copied beside every
# program that needs them; Redist\vc_redist.x64.exe is there too, for Steam's installer settings).
#
# The local AI (AI\ + Tools\VoiceServer.*) is copied beside the game; the game starts it and ends it (HUProcess), so
# suspects think, listen and speak with nothing for the player to set up, on any maker's graphics card: the brain runs
# through Vulkan (or on the processor), speech recognition on the processor, and the voices are the big ones on an
# NVIDIA card with 12 GB and the small one (AI\voice\supertonic, on the processor) everywhere else. AI\README.txt has
# the list. Without the AI the game still plays: suspects answer with written lines. -NoAI makes that small build.
#
# Usage: powershell -ExecutionPolicy Bypass -File Tools\Package.ps1 [-Config Development] [-NoAI]
#   Shipping (default) has no console and writes no log; Development keeps both, for chasing a packaged-only bug.
param([ValidateSet("Shipping", "Development")][string]$Config = "Shipping", [switch]$NoAI)
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
$Engine = "C:\Program Files\Epic Games\UE_5.8"
$Out = Join-Path $Root "Packaged"
# which build this is (the build stamps it too; this is for the folder and the summary)
$Version = & "$PSScriptRoot\StampVersion.ps1"

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
    # what a PC without a big NVIDIA card runs on: say so here, rather than ship a game that is silent for those players
    $Needs = [ordered]@{
        "llama\ggml-vulkan.dll"                 = "the brain can't use AMD or Intel graphics cards (llama.cpp's Vulkan build isn't the one installed)"
        "voice\supertonic\tts.json"             = "PCs without a 12 GB NVIDIA card get no voices (the small voice isn't installed)"
        "voice\bin\sherpa-onnx-offline-tts.exe" = "PCs without a 12 GB NVIDIA card get no voices (sherpa-onnx's speech program isn't installed)"
    }
    foreach ($n in $Needs.Keys) { if (-not (Test-Path "$Proj\AI\$n")) { Write-Warning "AI\$n is missing: $($Needs[$n])." } }
} else {
    Write-Warning "AI\llama isn't installed here, so the build has no local AI (suspects use written lines)."
}

# Microsoft's Visual C++ runtime, app-local (Visual Studio's redistribution terms allow it): the game, llama-server and
# Python's packages (torch, numpy...) all load it, and a PC that has never installed a game may not have it.
# Windows looks in a program's own folder first, so these copies are what they use.
$Vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath 2>$null
$Crt = if ($Vs) { Get-ChildItem "$Vs\VC\Redist\MSVC" -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^\d+(\.\d+)+$' } | Sort-Object { [version]$_.Name } | Select-Object -Last 1 }
if ($Crt) {
    $Dlls = @(Get-ChildItem "$($Crt.FullName)\x64\Microsoft.VC*.CRT\*.dll") + @(Get-ChildItem "$($Crt.FullName)\x64\Microsoft.VC*.OpenMP\vcomp140.dll" -ErrorAction SilentlyContinue)
    $Dirs = @("$Proj\Binaries\Win64") + @(if (-not $NoAI) { "$Proj\AI\llama", "$Proj\AI\python" })
    foreach ($d in $Dirs) { if (Test-Path $d) { Copy-Item $Dlls.FullName $d -Force } }
    "Visual C++ runtime $($Crt.Name): $($Dlls.Count) DLLs beside the game$(if (-not $NoAI) { ', llama-server and Python' })"
} else {
    Write-Warning "Visual Studio's redistributable folder wasn't found: a PC without the Visual C++ runtime will need Redist\vc_redist.x64.exe first."
}

# UE 5.8 no longer stages its prerequisites installer; Microsoft's Visual C++ runtime installer goes beside the game too
New-Item -ItemType Directory -Force "$Game\Redist" | Out-Null
Copy-Item "$Engine\Engine\Extras\Redist\en-us\vc_redist.x64.exe" "$Game\Redist" -Force

# never ship anyone's paid-voice key
if (Test-Path "$Proj\ElevenLabsKey.txt") { Remove-Item "$Proj\ElevenLabsKey.txt" -Force }

# testers quote this; it's also in the corner of the menu and in crash reports
"Homicide Unit $Version, $Config, packaged $(Get-Date -Format 'yyyy-MM-dd HH:mm')" | Set-Content -Encoding UTF8 "$Game\Version.txt"

$GB = (Get-ChildItem $Game -Recurse -File | Measure-Object Length -Sum).Sum / 1GB
"Packaged $Config $Version`: $Game\HomicideUnit.exe ({0:N1} GB)" -f $GB
