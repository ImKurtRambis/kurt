// The MOTOR POOL door in the squad room: the way back out to an open case. Back from the scene, the squad can work
// the case board, then gather at this door and press E to drive back to the scene together - to bag what they missed,
// question someone again, or bring another suspect in. It's a cruiser (same gathering and travel rules) drawn as a
// door. On the second visit the scene leaves out the evidence already bagged and the suspects already in holding.
#pragma once

#include "CoreMinimal.h"
#include "HUCruiser.h"
#include "HUMotorPoolDoor.generated.h"

UCLASS()
class HOMICIDEUNIT_API AHUMotorPoolDoor : public AHUCruiser
{
	GENERATED_BODY()
public:
	AHUMotorPoolDoor();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
private:
	float TestClock = 0.f; // -HUReturnTest (see Tick)
};
