#pragma once

#include "CoreMinimal.h"
#include "InventorySlot.generated.h"

class UTexture2D;

// ============================================================================
//  FInventorySlot – anzeige-fertige Sicht auf einen Inventar-Eintrag (für UMG).
//  Auflösung von Name/Preis/Icon erfolgt über die Item-Definitionen des GameMode.
// ============================================================================
USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	TObjectPtr<UTexture2D> Icon = nullptr;
};
