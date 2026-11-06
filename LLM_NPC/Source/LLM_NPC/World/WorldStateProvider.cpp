#include "WorldStateProvider.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "WorldClock.h"
#include "WorldLocationManager.h"
#include "WorldLocationDataAsset.h"
#include "LLM_NPC/Economy/MarketStall.h"
#include "LLM_NPC/NPC/NPCCharacter.h"

UWorldStateProvider* UWorldStateProvider::Get(const UObject* _worldContext)
{
	if (UGameInstance* gi = UGameplayStatics::GetGameInstance(_worldContext))
		return gi->GetSubsystem<UWorldStateProvider>();
	return nullptr;
}

UWorld* UWorldStateProvider::GetWorldSafe() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

// ============================================================================
//  Ebene 1 – Zeit
// ============================================================================
FString UWorldStateProvider::GetTimeState() const
{
	UWorld* w = GetWorldSafe();
	const UWorldClock* clock = w ? w->GetSubsystem<UWorldClock>() : nullptr;
	if (!clock) return FString();

	const int32 h = clock->GetHour();
	const TCHAR* phase =
		(h < 6)  ? TEXT("night")     :
		(h < 12) ? TEXT("morning")   :
		(h < 18) ? TEXT("afternoon") :
		(h < 22) ? TEXT("evening")   : TEXT("night");

	return FString::Printf(TEXT("It is %02d:00 on %s (%s)."),
		h, *clock->GetWeekdayName(), phase);
}

// ============================================================================
//  Ebene 2 – Orte
// ============================================================================
FString UWorldStateProvider::GetPlacesState() const
{
	const UGameInstance* gi = GetGameInstance();
	const UWorldLocationManager* loc = gi ? gi->GetSubsystem<UWorldLocationManager>() : nullptr;
	if (!loc || !loc->LocationData) return FString();

	TArray<FString> names;
	for (const FWorldLocation& l : loc->LocationData->Locations)
		if (!l.Name.IsEmpty())
			names.Add(l.Name);

	if (names.Num() == 0) return FString();
	return FString::Printf(TEXT("The market square is the heart of the village. Known places: %s."),
		*FString::Join(names, TEXT(", ")));
}

// ============================================================================
//  Ebene 3 – Wirtschaft (offene Marktstände)
// ============================================================================
FString UWorldStateProvider::GetEconomyState() const
{
	UWorld* w = GetWorldSafe();
	if (!w) return FString();

	TArray<FString> open;
	for (TActorIterator<AMarketStall> it(w); it; ++it)
	{
		AMarketStall* stall = *it;
		if (!stall || !stall->IsOpenNow()) continue;

		const FString vendor = stall->Vendor ? stall->Vendor->GetNPCName() : FString();
		open.Add(vendor.IsEmpty()
			? stall->StallName
			: FString::Printf(TEXT("%s (%s)"), *stall->StallName, *vendor));
	}

	if (open.Num() == 0)
		return TEXT("No market stalls are open right now.");
	return FString::Printf(TEXT("Open market stalls right now: %s."), *FString::Join(open, TEXT(", ")));
}

// ============================================================================
//  Kompletter Block (Präfix für jeden Prompt)
// ============================================================================
FString UWorldStateProvider::BuildBlock() const
{
	const FString time   = GetTimeState();
	const FString places = GetPlacesState();
	const FString econ   = GetEconomyState();

	FString block = TEXT("WORLD STATE:\n");
	if (!time.IsEmpty())   block += FString::Printf(TEXT("- %s\n"), *time);
	if (!places.IsEmpty()) block += FString::Printf(TEXT("- %s\n"), *places);
	if (!econ.IsEmpty())   block += FString::Printf(TEXT("- %s\n"), *econ);
	block += TEXT("\n");
	return block;
}
