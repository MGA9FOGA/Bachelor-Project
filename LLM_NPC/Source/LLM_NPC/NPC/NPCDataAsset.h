#pragma once
#include "CoreMinimal.h"
#include "Components/NPCWorldEnums.h"
#include "NPCDataAsset.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UStaticMesh;

// ============================================================
//  Statisches Anbauteil (Frisur, Bart, Augenbrauen, Hut …).
//  Wird an einen Knochen/Socket des Körper-Skeletts gehängt und folgt dessen
//  Bewegung (kein Skelett/Anim nötig, weil starr).
// ============================================================
USTRUCT(BlueprintType)
struct FCharacterAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Socket = TEXT("head");

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTransform RelativeTransform = FTransform::Identity;
};

// ============================================================
//  Welche ECS-Systeme soll dieser NPC ausführen?
//  Bitmask: mehrere im Editor ankreuzbar
// ============================================================
UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class ENPCSystemFlags : uint8
{
	None        = 0        UMETA(Hidden),
	MemoryDecay = 1 << 0   UMETA(DisplayName = "Memory Decay"),
	EmotionDecay= 1 << 1   UMETA(DisplayName = "Emotion Decay"),
	Needs       = 1 << 2   UMETA(DisplayName = "Needs Update"),
	Wander      = 1 << 3   UMETA(DisplayName = "Wander/Movement"),
	TransformSync=1 << 4   UMETA(DisplayName = "Transform Sync"),
	Schedule    = 1 << 5   UMETA(DisplayName = "Schedule")  
};

// ============================================================
//  Schlaf-Typ (Chronotyp) – nur ein HINWEIS für das LLM (typische Schlaf-/
//  Wachzeit) und die "nahe Schlafenszeit"-Logik. Das LLM plant Schlafen/
//  Aufwachen selbst. Wird im DataAsset pro NPC gesetzt.
// ============================================================
UENUM(BlueprintType)
enum class ENPCSleepType : uint8
{
	EarlyBird   UMETA(DisplayName = "Frühaufsteher (21–5 Uhr)"),
	Normal      UMETA(DisplayName = "Normal (22–6 Uhr)"),
	LateSleeper UMETA(DisplayName = "Langschläfer (0–8 Uhr)"),
	NightOwl    UMETA(DisplayName = "Nachtmensch (2–9 Uhr)")
};

// Stunde, zu der dieser Typ schlafen geht (0–23).
static int32 SleepHourForType(ENPCSleepType _type)
{
	switch (_type)
	{
	case ENPCSleepType::EarlyBird:   return 21;
	case ENPCSleepType::Normal:      return 22;
	case ENPCSleepType::LateSleeper: return 0;
	case ENPCSleepType::NightOwl:    return 2;
	default:                         return 22;
	}
}

// Stunde, zu der dieser Typ aufwacht (0–23).
static int32 WakeHourForType(ENPCSleepType _type)
{
	switch (_type)
	{
	case ENPCSleepType::EarlyBird:   return 5;
	case ENPCSleepType::Normal:      return 6;
	case ENPCSleepType::LateSleeper: return 8;
	case ENPCSleepType::NightOwl:    return 9;
	default:                         return 6;
	}
}

UCLASS(BlueprintType)
class LLM_NPC_API UNPCDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// === Identity — geht in FIdentityData ===
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FString NPCName = TEXT("Unknown");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	ENPCOccupation Occupation = ENPCOccupation::Villager;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	EWorldLocationTag HomeTag = EWorldLocationTag::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity",
			  meta=(MultiLine=true))
	FString Personality;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	TArray<FString> Goals;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	EWorldLocationTag WorkTag = EWorldLocationTag::None;

	// === Needs — geht in FNeedsData ===
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Needs",
			  meta=(ClampMin=0.0f, ClampMax=1.0f))
	float BaseHunger = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Needs",
			  meta=(ClampMin=0.0f, ClampMax=1.0f))
	float BaseExhaustion = 0.2f;

	// === Aussehen (modulare Quaternius-Chars) ===
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TObjectPtr<USkeletalMesh> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<TObjectPtr<USkeletalMesh>> OutfitPieces;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSubclassOf<UAnimInstance> AnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FCharacterAttachment> Attachments;

	// === ECS-Systeme — welche soll dieser NPC ausführen? ===
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ECS Systems",
			  meta=(Bitmask, BitmaskEnum="/Script/LLM_NPC.ENPCSystemFlags"))
	int32 ActiveSystems = 0;

	// === Tagesablauf (LLM-Hinweise) ===
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Schedule",
			  meta=(MultiLine=true))
	FString RoutineDescription;

	// Schlaf-Typ: nur ein HINWEIS für das LLM (typische Schlaf-/Wachzeit) 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Schedule")
	ENPCSleepType SleepType = ENPCSleepType::Normal;

	// === Hilfsmethode für ECS ===
	FString GetOccupationString() const
	{
		const UEnum* E = StaticEnum<ENPCOccupation>();
		return E ? E->GetDisplayNameTextByValue((int64)Occupation).ToString()
				 : TEXT("Villager");
	}
};