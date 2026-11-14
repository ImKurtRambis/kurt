#include "HomicideUnit.h"
#include "HUVersion.h"
#include "Modules/ModuleManager.h"

// the game module: says which build this is as soon as it loads (the log, and crash reports)
class FHomicideUnitModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() override { HUVersion::Announce(); }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FHomicideUnitModule, HomicideUnit, "HomicideUnit");
