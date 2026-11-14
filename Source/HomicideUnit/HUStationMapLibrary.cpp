#include "HUStationMapLibrary.h"
#include "Components/ChildActorComponent.h"
#include "GameFramework/Actor.h"

void UHUStationMapLibrary::RefreshBuilder(AActor* Builder)
{
	if(!IsValid(Builder)) return;
#if WITH_EDITOR
	Builder->RerunConstructionScripts();
	TInlineComponentArray<UChildActorComponent*> Children(Builder);
	for(UChildActorComponent* Child : Children) if(Child) RefreshBuilder(Child->GetChildActor());
#endif
}
