#pragma once
#include "CoreMinimal.h"
#include "NPCWorldEnums.generated.h"

UENUM(BlueprintType)
enum class ENPCOccupation : uint8
{
	Villager    UMETA(DisplayName = "Villager"),
	Blacksmith  UMETA(DisplayName = "Blacksmith"),
	Merchant    UMETA(DisplayName = "Merchant"),
	Guard       UMETA(DisplayName = "Guard")
};

// ============================================================
//  Aktueller Wunsch/Tätigkeit eines NPCs — von NPC UND World genutzt
//  (z.B. als Schlüssel für aktionsabhängige Spot-Animationen).
// ============================================================
UENUM(BlueprintType)
enum class ENPCDesire : uint8
{
	None    UMETA(DisplayName = "None"),
	Idle    UMETA(DisplayName = "Idle"),
	Wander  UMETA(DisplayName = "Wander"),
	Eat     UMETA(DisplayName = "Eat"),
	Sleep   UMETA(DisplayName = "Sleep"),
	Rest    UMETA(DisplayName = "Rest"),
	Work    UMETA(DisplayName = "Work"),
	Talk    UMETA(DisplayName = "Talk"),
	Drink   UMETA(DisplayName = "Drink"),  
	Shop    UMETA(DisplayName = "Shop")   
};

// ============================================================
//  Orte-Enum — einmal definiert, überall verwendet
// ============================================================
UENUM(BlueprintType)
enum class EWorldLocationTag : uint8
{
	None        UMETA(DisplayName = "None"),
	Walking     UMETA(DisplayName = "Walking"),
	Blacksmith  UMETA(DisplayName = "Schmiede"),
	Market      UMETA(DisplayName = "Marktplatz"),
	Tavern      UMETA(DisplayName = "Taverne"),
	Well        UMETA(DisplayName = "Brunnen"),
	Church      UMETA(DisplayName = "Kirche"),
	Farm        UMETA(DisplayName = "Farm"),
	GuardPost   UMETA(DisplayName = "Wachposten"),
	Home        UMETA(DisplayName = "Zuhause"),
	Inn         UMETA(DisplayName = "Gasthaus"),
	House       UMETA(DisplayName = "Haus")
};