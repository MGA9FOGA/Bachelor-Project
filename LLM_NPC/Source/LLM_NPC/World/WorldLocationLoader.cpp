#include "WorldLocationLoader.h"
#include "WorldLocationManager.h"

void AWorldLocationLoader::BeginPlay()
{
	Super::BeginPlay();

	if (!LocationData) return;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorldLocationManager* mgr =
			GI->GetSubsystem<UWorldLocationManager>())
		{
			mgr->LocationData = LocationData;
		}
	}
}
