#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LLM_NPC/NPC/Components/NPCWorldEnums.h"
#include "WorldSpot.generated.h"

class UAnimMontage;
class UArrowComponent;
class UStaticMeshComponent;

// Welche Art von Platz ist das?
UENUM(BlueprintType)
enum class ESpotType : uint8
{
	Work   UMETA(DisplayName = "Work"),
	Sleep  UMETA(DisplayName = "Sleep"),
	Eat    UMETA(DisplayName = "Eat"),
	Idle   UMETA(DisplayName = "Idle"),
	Rest   UMETA(DisplayName = "Rest"),  
	Shop   UMETA(DisplayName = "Shop")   
};

/**
 * AWorldSpot – ein im Level platzierter Platz mit sichtbarem Objekt
 * (Amboss, Bett, Stuhl, Tisch, Lagerfeuer …).
 *
 * - Mesh         = das sichtbare Objekt
 * - StandSlots   = ein oder MEHRERE Steh-Slots (wo NPCs stehen + hinschauen)
 * - PlayerSlot   = wo der Spieler beim Interagieren steht
 *
 * Capacity bestimmt, wie viele NPCs gleichzeitig hier Platz finden (z.B. eine
 * lange Bank oder ein Tisch mit mehreren Plätzen). Jeder NPC bekommt einen
 * eigenen Steh-Slot. Slot 0 = NPCStandSlot (rückwärtskompatibel).
 */
UCLASS()
class LLM_NPC_API AWorldSpot : public AActor
{
	GENERATED_BODY()

public:
	AWorldSpot();

	// === Konfiguration ===
	UPROPERTY(EditAnywhere, Category = "Spot")
	ESpotType Type = ESpotType::Work;

	UPROPERTY(EditAnywhere, Category = "Spot")
	EWorldLocationTag Building = EWorldLocationTag::None;

	UPROPERTY(EditAnywhere, Category = "Spot", meta=(ClampMin=1, ClampMax=6))
	int32 Capacity = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spot")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// Slot 0: wo der erste NPC steht + hinschaut (rückwärtskompatibel)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spot")
	TObjectPtr<USceneComponent> NPCStandSlot;

	// Alle Steh-Slots (StandSlots[0] == NPCStandSlot). Im Editor positionieren.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spot")
	TArray<TObjectPtr<USceneComponent>> StandSlots;

	// Wo der Spieler beim Interagieren steht (optional)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spot")
	TObjectPtr<USceneComponent> PlayerSlot;

	// === Animation (liegt am Spot) ===
	// EINE kombinierte Montage mit drei Sections, damit die Übergänge LÜCKENLOS sind
	// (kein Montage-Wechsel → kein Frame Basispose dazwischen):
	//
	//   [Intro]  Stehen→Sitzen         (Next Section = Loop)
	//   [Loop]   sitzende Tätigkeit     (Next Section = Loop  → loopt auf sich selbst)
	//   [Outro]  Sitzen→Stehen         (Next Section = None → endet)
	//
	UPROPERTY(EditAnywhere, Category = "Spot|Animation")
	TObjectPtr<UAnimMontage> SpotMontage;

	// Optionale, eigene Montage fürs TRINKEN am selben Spot (gleiche Section-Namen
	// Intro/Loop/Outro). Nur an Eat-Spots relevant → Feld wird nur bei Type==Eat
	// angezeigt. Kommt ein NPC zum Trinken und ist diese gesetzt, wird sie statt
	// SpotMontage gespielt. Leer → wie Essen.
	UPROPERTY(EditAnywhere, Category = "Spot|Animation",
		meta=(EditCondition="Type == ESpotType::Eat", EditConditionHides))
	TObjectPtr<UAnimMontage> DrinkMontage;

	UAnimMontage* GetMontageForGoal(ENPCDesire _goal) const
	{
		return (_goal == ENPCDesire::Drink && DrinkMontage) ? DrinkMontage : SpotMontage;
	}

	UPROPERTY(EditAnywhere, Category = "Spot|Animation")
	FName IntroSection = TEXT("Intro");   // Ende = Start der Needs-Erholung

	UPROPERTY(EditAnywhere, Category = "Spot|Animation")
	FName OutroSection = TEXT("Outro");   

	UPROPERTY(EditAnywhere, Category = "Spot|Animation")
	FVector IntroMeshOffset = FVector::ZeroVector;

	// === Steh-Slots (NPCs) ===
	FTransform GetStandTransformFor(uint32 _entity) const;
	FTransform GetStandTransform() const { return GetStandTransformFor(0); }

	// === Reservierung (mehrere Slots, 0 = frei) ===
	bool IsFree() const;                  // mindestens ein Slot frei?
	bool TryReserve(uint32 _entity);      // belegt einen freien Slot
	void Release(uint32 _entity);

protected:
	virtual void BeginPlay() override;

private:
	static constexpr int32 MaxSlots = 6;

	// Nutzbare Slot-Anzahl (Capacity geklemmt auf die vorhandenen Slots).
	int32 UsableSlots() const;

	UPROPERTY(VisibleAnywhere, Category = "Spot")
	TObjectPtr<USceneComponent> m_root;

#if WITH_EDITORONLY_DATA
	// Zeigt die NPC-Blickrichtung im Editor (je Slot ein Pfeil)
	UPROPERTY()
	TArray<TObjectPtr<UArrowComponent>> m_facingArrows;
#endif

	// Pro Slot die belegende NPC-Entity (0 = frei). Index = Slot-Index.
	TArray<uint32> m_occupants;
};
