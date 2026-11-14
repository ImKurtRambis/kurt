// WHICH BUILD THIS IS: stamped from git before every build (Tools/StampVersion.ps1 -> HUBuildInfo.h). Shown in the
// corner of the desk menu, written at the top of the log, and put in crash reports (game data "HUBuild"), so a tester
// can say which build they played and a crash matches its build.
//   build N   commits on the branch; goes up with every commit (what testers quote)
//   (hash)    the exact commit; "+local" when the build had uncommitted changes
#pragma once

#include "CoreMinimal.h"

namespace HUVersion
{
	HOMICIDEUNIT_API int32 Number();
	// "build 412 (a24ad51)" / "build 412 (a24ad51 +local)"
	HOMICIDEUNIT_API FString Text();
	// Text() plus the commit date and the configuration: "build 412 (a24ad51), 2026-11-14, Shipping"
	HOMICIDEUNIT_API FString Full();
	// at start-up: the log line and the crash-report data
	void Announce();
}
