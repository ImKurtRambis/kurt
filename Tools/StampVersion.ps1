# Stamps the build with its version, from git: Source/HomicideUnit/HUBuildInfo.h (not in git).
# Runs before every build (PreBuildSteps in Source/*.Target.cs) and rewrites the header only when it changes, so it
# costs one small recompile at most. Prints the version, e.g. "build 412 (a24ad51)" - Package.ps1 uses that.
#   build N   commits on this branch (goes up with every commit; what testers quote)
#   (hash)    the exact commit; "+local" when the build had changes that weren't committed
param([switch]$Quiet)
$Root = Split-Path $PSScriptRoot -Parent
$Header = Join-Path $Root "Source\HomicideUnit\HUBuildInfo.h"

$Number = 0; $Commit = "unknown"; $Date = ""; $Dirty = 0
try {
    $Number = [int](git -C $Root rev-list --count HEAD 2>$null)
    $Commit = (git -C $Root rev-parse --short=7 HEAD 2>$null).Trim()
    $Date = (git -C $Root log -1 --format=%cs 2>$null).Trim()
    if (git -C $Root status --porcelain --untracked-files=no 2>$null) { $Dirty = 1 }
} catch { }
if (-not $Commit) { $Commit = "unknown" }

$Text = @"
// Written by Tools/StampVersion.ps1 before every build - don't edit, not in git.
#pragma once
#define HU_BUILD_NUMBER $Number
#define HU_BUILD_COMMIT TEXT("$Commit")
#define HU_BUILD_DATE TEXT("$Date")
#define HU_BUILD_LOCAL $Dirty
"@
$Old = if (Test-Path $Header) { [IO.File]::ReadAllText($Header) } else { "" }
if ($Old -ne $Text) { [IO.File]::WriteAllText($Header, $Text) }

$Version = "build $Number ($Commit$(if ($Dirty) { ' +local' }))"
if (-not $Quiet) { $Version }
