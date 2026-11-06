// WorldLocationLoader.h — einmal im Level platzieren
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldLocationDataAsset.h"
#include "WorldLocationLoader.generated.h"

class UWorldLocationManager;

UCLASS()
class LLM_NPC_API AWorldLocationLoader : public AActor
{
	GENERATED_BODY()

public:
	// Im Editor das DataAsset hier zuweisen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locations")
	TObjectPtr<UWorldLocationDataAsset> LocationData;

protected:
	virtual void BeginPlay() override;
};