#pragma once

#include "CoreMinimal.h"
#include "QuestData.generated.h"

// Quest-Art. Fetch: Items zum Geber bringen (Abgabe bei GiverName, Item-Check).
// Delivery: eine Nachricht (Description) zu einem anderen NPC (TargetNpc) tragen –
// Abgabe erfolgt bei TargetNpc, ohne Item, dafür merkt der sich die Nachricht.
UENUM(BlueprintType)
enum class EQuestType : uint8
{
	Fetch    UMETA(DisplayName="Fetch"),
	Delivery UMETA(DisplayName="Delivery")
};

// ============================================================================
//  FQuestData – eine vom LLM erzeugte Quest.
//  Fetch: der Geber verlangt TargetCount Stück von TargetItem; bei Abgabe RewardGold.
//
//  Delivery: der Spieler trägt Description als Nachricht zu TargetNpc; dieser merkt
//  sich die Nachricht, reflektiert und plant ggf. seinen Tag neu.
// ============================================================================
USTRUCT(BlueprintType)
struct FQuestData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FString QuestId;

	// Art der Quest (Fetch = Item bringen, Delivery = Nachricht überbringen).
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	EQuestType Type = EQuestType::Fetch;

	// Name des NPCs, der die Quest vergeben hat (Fetch-Abgabe erfolgt bei ihm).
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FString GiverName;

	// Boten-Quest: Name des NPCs, zu dem die Nachricht getragen wird (Abgabe bei ihm).
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FString TargetNpc;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FString Title;

	// Fetch: warum der Geber die Items braucht. Delivery: der Nachrichtentext selbst.
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FString Description;

	// Zu bringendes Item (ItemId) + Menge – nur für Fetch.
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FName TargetItem;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	int32 TargetCount = 1;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	int32 RewardGold = 0;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	bool bCompleted = false;
};
