#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "NPCInteractionTasks.generated.h"

class ADoor;

// ============================================================================
// TASK: CONVERSE – NPC↔NPC-Gespräch (Dialog-Gruppe)
// ============================================================================
/**
 * UBTTask_Converse – versucht ein Gespräch mit einem nahen, freien NPC zu beginnen
 * (über ANPCCharacter::TryStartNPCConversation) und läuft, bis es vorbei ist.
 * Kein Partner frei → Success (BT läuft weiter). Ersetzt den Tick-Trigger, wenn
 * am NPC bAutoStartConversations aus ist.
 */
UCLASS()
class LLM_NPC_API UBTTask_Converse : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Converse();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory, float _deltaSeconds) override;
};

// ============================================================================
// TASK: OPEN DOOR – Tür öffnen (Navigation/Interaktion-Gruppe)
// ============================================================================
/**
 * UBTTask_OpenDoor – öffnet die nächste Tür in Reichweite und wartet, bis sie offen
 * ist (oder MaxWait abläuft). Keine/schon offene Tür → sofort Success.
 */
UCLASS()
class LLM_NPC_API UBTTask_OpenDoor : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_OpenDoor();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory, float _deltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="NPC")
	float SearchRadius = 220.0f;

	UPROPERTY(EditAnywhere, Category="NPC")
	float MaxWait = 3.0f;

private:
	TWeakObjectPtr<ADoor> m_door;
	float m_waitTimer = 0.0f;
};

// ============================================================================
// TASK: BUY AT MARKET – am Stand einkaufen (Navigation/Interaktion-Gruppe)
// ============================================================================
/**
 * UBTTask_BuyAtMarket – kauft am nächsten offenen Marktstand in Reichweite die erste
 * bezahlbare Ware (der NPC gibt Gold, bekommt das Item, der Vendor verdient).
 * Voraussetzung: der NPC ist bereits nah am Stand (davor per GoToLocation navigieren).
 */
UCLASS()
class LLM_NPC_API UBTTask_BuyAtMarket : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_BuyAtMarket();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;

	// Umkreis (cm), in dem nach einem offenen Stand gesucht wird.
	UPROPERTY(EditAnywhere, Category="NPC")
	float SearchRadius = 300.0f;
};

// ============================================================================
// TASK: WANDER – ziellos schlendern, bevorzugt auf Wegen (Nicht-Gras-Layer)
// ============================================================================
/**
 * UBTTask_Wander – setzt ein zufälliges Wanderziel und Success (danach navigiert ein
 * GoToLocation-Task hin). Die Punkt-Wahl (Nicht-Gras-Fläche, kein Marktplatz-Bezug)
 * liegt zentral in ANPCCharacter::FindWanderTarget; das Gras-Material stellst du am
 * NPC über `WanderAvoidSurface` ein.
 */
UCLASS()
class LLM_NPC_API UBTTask_Wander : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Wander();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
};

// ============================================================================
// TASK: REFLECT – Reflexion anstoßen (LLM-Kommunikation)
// ============================================================================
/**
 * UBTTask_Reflect – stößt eine Reflexion an (z.B. während Ruhe/Idle), sofern seit der
 * letzten Reflexion genug „passiert" ist (FReflectionData::ImportanceSum). Async,
 * best-effort; ohne neue Ereignisse einfach Success.
 */
UCLASS()
class LLM_NPC_API UBTTask_Reflect : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Reflect();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;

	UPROPERTY(EditAnywhere, Category="NPC")
	float MinImportance = 0.5f;
};

// ============================================================================
// TASK: PLAN DAY – Tagesablauf vom LLM neu planen lassen (LLM-Kommunikation)
// ============================================================================
/**
 * UBTTask_PlanDay – stößt eine neue LLM-Tagesplanung an (ANPCCharacter::ReplanSchedule).
 */
UCLASS()
class LLM_NPC_API UBTTask_PlanDay : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PlanDay();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
};

// ============================================================================
// TASK: DECIDE GOAL – nächstes Ziel aus Needs/Plan wählen (Hilfstask)
// ============================================================================
/**
 * UBTTask_DecideGoal – lässt den NPC das nächste Ziel bestimmen
 * (ANPCCharacter::ChooseNextDestination: Arbeitsplatz/Bett/Ess-Spot oder Wander).
 * Setzt das Navigationsziel → Success (danach navigiert ein GoToLocation-Task hin).
 */
UCLASS()
class LLM_NPC_API UBTTask_DecideGoal : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DecideGoal();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
};
