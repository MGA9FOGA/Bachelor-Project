#pragma once
#include "CoreMinimal.h"
#include "WorldLocationDataAsset.h"
#include "LLM_NPC/NPC/Components/NPC_ECS.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldLocationManager.generated.h"


class UWorldLocationDataAsset;
class AWorldSpot;
enum class ESpotType : uint8;

UCLASS()
class LLM_NPC_API UWorldLocationManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
private:
	// Belegte Plätze (Work UND Sleep), Key = "<PREFIX>_<Tag>_<Index>"
	TMap<FString, EntityID> m_occupiedSpots;

	// Generischer Kern für jede Platz-Reservierung
	bool ReserveSpot(const TArray<FWorkstation>& _spots, EWorldLocationTag _tag,
	                 const FString& _prefix, EntityID _entity, const FVector& _basePos,
	                 FVector& _outWorldPos, FString& _outAnimTag);
	void ReleaseSpot(const FString& _prefix, EWorldLocationTag _tag, EntityID _entity);

public:
	virtual void Initialize(FSubsystemCollectionBase& _collection) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locations")
	TObjectPtr<UWorldLocationDataAsset> LocationData;

	bool GetLocationForNPC(ENPCOccupation _occupation,
											   float _hunger, float _exhaustion,
											   FWorldLocation& _outLocation) const;

	bool GetRandomLocation(EWorldLocationTag _excludeTag,
						   FWorldLocation& _outLocation) const;

	bool GetLocationByTag(EWorldLocationTag _tag,
						  FWorldLocation& _outLocation) const;
	
	bool GetLocationForDesire(ENPCDesire _desire, ENPCOccupation _occupation,
						  FWorldLocation& _outLocation) const;
	
	bool ReserveWorkstation(EWorldLocationTag _tag, EntityID _entity,
						FVector& _outWorldPos, FString& _outAnimTag);

	void ReleaseWorkstation(EWorldLocationTag _tag, EntityID _entity);

	bool ReserveSleepSpot(EWorldLocationTag _tag, EntityID _entity,
						FVector& _outWorldPos, FString& _outAnimTag);
	void ReleaseSleepSpot(EWorldLocationTag _tag, EntityID _entity);

	void RegisterSpot(AWorldSpot* _spot);

	// Freien Spot mit passendem Gebäude + Typ finden und für _entity reservieren.
	// Gibt den Spot zurück (Transform + Animation) oder nullptr, wenn keiner frei.
	AWorldSpot* ReserveSpotActor(EWorldLocationTag _building, ESpotType _type, EntityID _entity);

	// Wie ReserveSpotActor, aber GEBÄUDE-UNABHÄNGIG: den nächstgelegenen freien
	// Spot dieses Typs reservieren (z.B. irgendeine freie Bank in der Nähe).
	AWorldSpot* ReserveNearestSpotActor(ESpotType _type, const FVector& _from, EntityID _entity);

private:
	UPROPERTY()
	TArray<TObjectPtr<AWorldSpot>> m_spots;
};