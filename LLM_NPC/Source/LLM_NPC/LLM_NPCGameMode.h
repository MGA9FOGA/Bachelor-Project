// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/TimerHandle.h"
#include "LLM_NPC/Economy/InventoryData.h"
#include "LLM_NPC/Economy/InventorySlot.h"
#include "LLM_NPCGameMode.generated.h"

class UItemDataAsset;

UCLASS(minimalapi)
class ALLM_NPCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALLM_NPCGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Economy")
	TArray<TObjectPtr<UItemDataAsset>> KnownItems;

	FString GetItemListString() const;

	UFUNCTION(BlueprintPure, Category="Economy")
	UItemDataAsset* FindItem(FName _itemId) const;

	TArray<FInventorySlot> MakeSlots(const FInventoryData& _inv) const;

protected:
	virtual void BeginPlay() override;

private:
	void LogMarketSelfCheck() const;
	FTimerHandle m_selfCheckTimer;
};



