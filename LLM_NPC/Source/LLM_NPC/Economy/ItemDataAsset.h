#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDataAsset.generated.h"

// Was für einen Bedarf ein konsumierbares Item deckt.
UENUM(BlueprintType)
enum class EConsumeType : uint8
{
	Food  UMETA(DisplayName = "Food"),   // senkt Hunger
	Drink UMETA(DisplayName = "Drink")   // senkt Erschöpfung (erfrischt)
};

// ============================================================================
//  UItemDataAsset – definiert einen Item-Typ (im Editor angelegt).
//  Das Inventar speichert nur die ItemId; hier hängen Anzeigename, Preis, Icon.
// ============================================================================
UCLASS(BlueprintType)
class LLM_NPC_API UItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Stabile ID – Schlüssel im Inventar. NICHT nachträglich ändern.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(MultiLine=true))
	FString Description;

	// Basispreis in Gold (ein Marktstand kann davon abweichen).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(ClampMin=0))
	int32 BasePrice = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	TObjectPtr<UTexture2D> Icon;

	// ===== Konsumierbar (Essen/Trinken) =====
	// Kann ein NPC (oder Spieler) das Item verzehren, um Hunger/Erschöpfung zu senken?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	bool bConsumable = false;

	// Deckt Hunger (Food) oder erfrischt (Drink).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable",
		meta=(EditCondition="bConsumable"))
	EConsumeType ConsumeType = EConsumeType::Food;

	// Wie stark der Bedarf beim Verzehr sinkt (0..1). 0.5 = kräftige Mahlzeit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable",
		meta=(EditCondition="bConsumable", ClampMin=0.0, ClampMax=1.0))
	float Nourishment = 0.5f;
};
