#pragma once
#include "CoreMinimal.h"
#include "LLM_NPC/NPC/Components/NPCWorldEnums.h"
#include "WorldLocationDataAsset.generated.h"


USTRUCT(BlueprintType)
struct FWorkstation
{
    GENERATED_BODY()

    // Name/Zweck (Amboss, Blasebalg...)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Workstation")
    FString Name;

    // Offset relativ zur Ort-Position — wo genau steht der NPC
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Workstation")
    FVector Offset = FVector::ZeroVector;

    // Welche Animation/Tätigkeit hier (z.B. "hammer", "pump")
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Workstation")
    FString AnimationTag;
};

// "Hingeh"-Aktivität für Desire-Matching (Eat/Social/Idle).
// Arbeiten/Schlafen laufen NICHT hierüber, sondern über Workstations/SleepSpots.
UENUM(BlueprintType)
enum class ELocationActivity : uint8
{
    Idle    UMETA(DisplayName = "Idle"),
    Eat     UMETA(DisplayName = "Eat"),
    Social  UMETA(DisplayName = "Social")
};

// ============================================================
//  Hilfsfunktion — Enum → FString für Blackboard "Target"
// ============================================================
FORCEINLINE FString WorldLocationTagToString(EWorldLocationTag _tag)
{
    const UEnum* E = StaticEnum<EWorldLocationTag>();
    return E ? E->GetNameStringByValue((int64)_tag) : TEXT("None");
}

USTRUCT(BlueprintType)
struct FWorldLocation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location")
    FString Name;

    // Enum statt String — kein Tippfehler möglich
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location")
    EWorldLocationTag Tag = EWorldLocationTag::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location")
    FVector Position = FVector::ZeroVector;

    // Enum statt String
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location")
    ELocationActivity Activity = ELocationActivity::Idle;

    // Welche Berufe bevorzugen diesen Ort
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location")
    TArray<ENPCOccupation> PreferredBy;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location")
    TArray<FWorkstation> Workstations;

    // Schlafplätze (Betten) – werden beim Schlafen reserviert, wie Workstations.
    // Pro Bett: Name, Offset zur Ort-Position, AnimationTag (z.B. "sleep").
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location")
    TArray<FWorkstation> SleepSpots;
};

UCLASS(BlueprintType)
class LLM_NPC_API UWorldLocationDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locations")
    TArray<FWorldLocation> Locations;
};