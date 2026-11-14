#include "HUVersion.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Misc/App.h"

// Written by Tools/StampVersion.ps1, which the build runs first (PreBuildSteps in Source/*.Target.cs). Always included,
// never optional: that way the build knows this file depends on it and recompiles it when the version changes.
// "Cannot open include file HUBuildInfo.h": run powershell -File Tools\StampVersion.ps1 once.
#include "HUBuildInfo.h"

int32 HUVersion::Number() { return HU_BUILD_NUMBER; }

FString HUVersion::Text()
{
	return FString::Printf(TEXT("build %d (%s%s)"), HU_BUILD_NUMBER, HU_BUILD_COMMIT, HU_BUILD_LOCAL ? TEXT(" +local") : TEXT(""));
}

FString HUVersion::Full()
{
	const FString Date = HU_BUILD_DATE;
	return Text() + (Date.IsEmpty() ? FString() : TEXT(", ") + Date) + TEXT(", ") + LexToString(FApp::GetBuildConfiguration());
}

void HUVersion::Announce()
{
	UE_LOG(LogTemp, Display, TEXT("Homicide Unit %s"), *Full());
	FGenericCrashContext::SetGameData(TEXT("HUBuild"), Full());
}
