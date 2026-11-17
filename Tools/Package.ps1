# Packages a standalone Windows build into Packaged\Windows. HomicideUnit.exe runs on a PC with nothing installed but
# Windows 10/11 and a graphics driver: no Unreal, no Python, no Visual C++ runtime (its DLLs are copied beside every
# program that needs them; Redist\vc_redist.x64.exe is there too, for Steam's installer settings).
#
# The local AI is copied beside the game (AI\); the game starts it and ends it (HUProcess), so suspects think, listen
# and speak with nothing for the player to set up. It is the same on every PC and sized for an 8 GB graphics card: one
# brain (through Vulkan on any maker's card, else on the processor), one voice and the ears (both on the processor),
# about 3.2 GB in all. Only what the game runs is copied - see $Ship below; AI\README.txt has the list.
# Without the AI the game still plays: suspects answer with written lines. -NoAI makes that small build.
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

# -ubtargs: left to itself, Unreal's build tool refuses to build while any editor has Live Coding on - even an editor
# open on another copy of the project - so packaging failed whenever one was open. These switches make it build
# anyway.
& "$Engine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\HomicideUnit.uproject" -platform=Win64 `
    "-clientconfig=$Config" -build -cook -stage -pak -iostore -compressed -prereqs -package -archive `
    "-archivedirectory=$Out" -nop4 -utf8output -unattended "-ubtargs=-NoHotReload -NoHotReloadFromIDE"
# ("ConflictingInstance" above means another build was running at that moment: run this again)
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
    # ONLY WHAT THE GAME RUNS: one brain, one voice, one pair of ears. A developer's AI folder can hold a lot more
    # (voices the game no longer uses, Python, a second listening model, logs): none of it is copied, and whatever an
    # older, bigger package left in here is taken out again.   folder -> the files wanted from it
    $Ship = [ordered]@{
        "llama"            = @("*")             # the brain's program: llama.cpp's Vulkan build
        "models"           = @("suspect.gguf")  # the brain
        "voice\bin"        = @("sherpa-onnx-offline-tts.exe", "sherpa-onnx-offline.exe", "sherpa-onnx-offline-websocket-server.exe", "onnxruntime*.dll") # sherpa-onnx: it speaks and it listens
        "voice\supertonic" = @("*")             # the voice
    }
    # the ears: the accurate model, or the small one where that isn't installed (the game only ever loads one of them)
    if (Test-Path "$Root\AI\stt-accurate\tokens.txt") { $Ship["stt-accurate"] = @("*") }
    elseif (Test-Path "$Root\AI\stt\tokens.txt") { $Ship["stt"] = @("*.onnx", "tokens.txt", "LICENSE") }
    $Wanted = @("README.txt")
    foreach ($d in $Ship.Keys) {
        if (-not (Test-Path "$Root\AI\$d")) { continue }
        $Files = $Ship[$d]
        # (robocopy only copies what changed, so packaging again is quick)
        robocopy "$Root\AI\$d" "$Proj\AI\$d" @Files /S /XF *.err *.out *.log /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "Copying AI\$d failed (robocopy exit code $LASTEXITCODE)." }
        $Wanted += foreach ($f in $Files) { Get-ChildItem "$Root\AI\$d" -Filter $f -File -Recurse | Where-Object { $_.Extension -notin ".err", ".out", ".log" } | ForEach-Object { $_.FullName.Substring("$Root\AI\".Length) } }
    }
    Copy-Item "$Root\AI\README.txt" "$Proj\AI" -Force
    Get-ChildItem "$Proj\AI" -Recurse -File | Where-Object { $_.FullName.Substring("$Proj\AI\".Length) -notin $Wanted } | Remove-Item -Force
    Get-ChildItem "$Proj\AI" -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending | Where-Object { -not (Get-ChildItem $_.FullName -Force) } | Remove-Item -Force
    if (Test-Path "$Proj\Tools") { Remove-Item "$Proj\Tools" -Recurse -Force } # (the voice server older builds carried)
    # say so here, rather than ship a game that is silent, deaf or slow for everyone
    $Needs = [ordered]@{
        "llama\ggml-vulkan.dll"                 = "the brain can't use a graphics card and runs on the processor, slowly (llama.cpp's Vulkan build isn't the one installed)"
        "models\suspect.gguf"                   = "suspects have no brain and answer with written lines only (the model isn't installed)"
        "voice\supertonic\tts.json"             = "nobody has a voice: every line is a caption (the voice isn't installed)"
        "voice\bin\sherpa-onnx-offline-tts.exe" = "nobody has a voice: every line is a caption (sherpa-onnx's speaking program isn't installed)"
        "voice\bin\sherpa-onnx-offline.exe"     = "the game can't hear the player: questions are typed (sherpa-onnx's listening program isn't installed)"
    }
    foreach ($n in $Needs.Keys) { if (-not (Test-Path "$Proj\AI\$n")) { Write-Warning "AI\$n is missing: $($Needs[$n])." } }
    if (-not ($Ship.Contains("stt-accurate") -or $Ship.Contains("stt"))) { Write-Warning "No listening model is installed (AI\stt-accurate): the game can't hear the player, questions are typed." }
    "The local AI: {0:N1} GB ({1})" -f ((Get-ChildItem "$Proj\AI" -Recurse -File | Measure-Object Length -Sum).Sum / 1GB), ($Ship.Keys -join ", ")
} else {
    Write-Warning "AI\llama isn't installed here, so the build has no local AI (suspects use written lines)."
}

# Microsoft's Visual C++ runtime, app-local (Visual Studio's redistribution terms allow it): the game and llama-server
# load it, and a PC that has never installed a game may not have it. Windows looks in a program's own folder first, so
# these copies are what they use. (sherpa-onnx's programs carry their own: checked with dumpbin /dependents.)
$Vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath 2>$null
$Crt = if ($Vs) { Get-ChildItem "$Vs\VC\Redist\MSVC" -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^\d+(\.\d+)+$' } | Sort-Object { [version]$_.Name } | Select-Object -Last 1 }
if ($Crt) {
    $Dlls = @(Get-ChildItem "$($Crt.FullName)\x64\Microsoft.VC*.CRT\*.dll") + @(Get-ChildItem "$($Crt.FullName)\x64\Microsoft.VC*.OpenMP\vcomp140.dll" -ErrorAction SilentlyContinue)
    $Dirs = @("$Proj\Binaries\Win64") + @(if (-not $NoAI) { "$Proj\AI\llama" })
    foreach ($d in $Dirs) { if (Test-Path $d) { Copy-Item $Dlls.FullName $d -Force } }
    "Visual C++ runtime $($Crt.Name): $($Dlls.Count) DLLs beside the game$(if (-not $NoAI) { ' and llama-server' })"
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
