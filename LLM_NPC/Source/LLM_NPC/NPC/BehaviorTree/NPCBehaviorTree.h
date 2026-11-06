#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BTTaskNode.h"
#include "LLM_NPC/NPC/Components/NPC_ECS.h"
#include "NPCBehaviorTree.generated.h"


class ANPCCharacter;
struct FNeedsData;
struct FEmotionData;
struct FNPCEntity;
/**
 * ============================================================================
 * NPC BEHAVIOR TREE SYSTEM
 * ============================================================================
 * 
 * 4 Simple Tasks:
 * 1. BTTask_GoToLocation - Navigate to target
 * 2. BTTask_Idle - Do nothing
 * 3. BTTask_CheckNeeds - Check if Hunger/Exhaustion critical
 * 4. BTTask_PerformAction - Execute action
 * 
 * Verbindung zum ECS:
 * - Liest: Memory, Emotion, Needs, NavMesh, BTState
 * - Schreibt: NavMesh.TargetLocation, BTState.ActiveTask
 */

// ============================================================================
// BLACKBOARD KEYS
// ============================================================================

struct FNPCBlackboardKeys
{
	// Input (Vom ECS)
	static FName CurrentHunger;			// FMemoryData->Nedds.Hunger
	static FName CurrentExhaustion;		// FEmotionData->Needs.Exhaustion
	static FName DominantEmotion;		// EEmotionType
	static FName TopMemory;				// FString (erste wichtigste Memory)
	
	// Output (zu NavMesh + BTState)
	static FName TargetLocation ;		// FVector (wohin navigieren)
	static FName CurrentAction;			// FString (was tun)
	static FName IsHungry;				// bool
	static FName IsExhausted;			// bool
	
	static FName PlayerTalking;			// bool - NPC stops when he is talking
};


// ============================================================================
// TASK: GO TO LOCATION
// ============================================================================

/**
 * BTTask_GoToLocation - Navigate to a target location
 * 
 * Logik:
 * 1. Liest TargetLocation vom Blackboard
 * 2. Setzt NavMesh.TargetLocation
 * 3. Wartet bis NPC ankommt (NavMesh.bTargetReached)
 * 4. Success wenn angekommen
 */

UCLASS()
class LLM_NPC_API UBTTask_GoToLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_GoToLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory, float _deltaSeconds) override;

private:
	FVector GetTargetLocation(UBehaviorTreeComponent& _ownerComp);
	bool HasReachedTarget(ANPCCharacter* _npc);
};

// ============================================================================
// TASK: IDLE
// ============================================================================

/**
 * BTTask_Idle - Do nothing, just wait
 * 
 * Logik:
 * 1. Setzt ActiveTask = "Idle"
 * 2. Stoppt Navigation
 * 3. Wartet X Sekunden
 * 4. Success nach Zeit vorbei
 */
UCLASS()
class LLM_NPC_API UBTTask_Idle : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_Idle();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory, float _deltaSeconds) override;

	UPROPERTY(EditAnywhere, Category= "NPC")
	float IdleDuration = 3.0f;
	
private:
	float IdleTimer = 0.0f;
};

// ============================================================================
// TASK: CHECK NEEDS
// ============================================================================

/**
 * BTTask_CheckNeeds - Check if NPC has critical needs
 * 
 * Logik:
 * 1. Liest Hunger und Exhaustion
 * 2. Setzt Blackboard Keys: IsHungry, IsExhausted
 * 3. Success immer (nur Status update)
 * 4. Gibt Info an BT für Decisions
 */

UCLASS()
class LLM_NPC_API UBTTask_CheckNeeds : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
	
private:
	void UpdateNeedsOnBlackboard(UBehaviorTreeComponent& _ownerComp, ANPCCharacter* _npc);
};

// ============================================================================
// TASK: PERFORM ACTION
// ============================================================================

/**
 * BTTask_PerformAction - Do something (eat, sleep, talk, work)
 * 
 * Logik:
 * 1. Liest CurrentAction vom Blackboard
 * 2. Setzt BTState.ActiveTask = CurrentAction
 * 3. Modifiziert Needs/Emotion entsprechend
 *    - Eating: Hunger -= 0.5
 *    - Sleeping: Exhaustion -= 0.7
 *    - Socializing: Emotion Happy += 0.3
 * 4. Wartet Duration
 * 5. Success nach Action vorbei
 */

UCLASS()
class LLM_NPC_API  UBTTask_PerformAction : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PerformAction();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory, float _deltaSeconds) override;

	UPROPERTY(EditAnywhere, Category= "NPC")
	float ActionDuration = 2.0f;
	
private:
	FString m_currentAction;
	float m_actionTimer = 0.0f;
};

// ============================================================================
// TASK: TALK (Interaktions-State)
// ============================================================================

/**
 * BTTask_Talk - NPC im Gesprächs-State
 *
 * Logik:
 * 1. Stoppt die Bewegung
 * 2. Dreht den NPC zum Spieler (GetTalkTarget)
 * 3. Setzt BTState.ActiveTask = "Talk" → vom AnimBP lesbar
 * 4. Läuft, bis PlayerTalking == false (Gespräch vorbei)
 */
UCLASS()
class LLM_NPC_API UBTTask_Talk : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Talk();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory, float _deltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "NPC")
	float TurnSpeed = 8.0f;
};

// ============================================================================
// NPC BEHAVIOR TREE MANAGER
// ============================================================================

/**
 * FNPCBT - Manages Behavior Tree execution for one NPC
 * 
 * Ähnlich wie FNPCECS:
 * - Hält BT + Blackboard für einen NPC
 * - Wird vom Character aufgerufen
 * - Updated die BT jedes Frame
 */

class FNPCBT
{
public:
	FNPCBT();
	~FNPCBT();
	
	void Initialize(class UBehaviorTreeComponent& _btComp, class UBlackboardComponent* _bbComp);
	void Update(float _deltatime);
	
	// Setze Blackboard Values (von ECS)
	void UpdateBlackboardFromECS(FNPCEntity* _entity);
	
	// Update Blackboard mit Locations (z.B. Tavern, Bed, etc)
	void SetWaypoint(const FString& _waypointName, const FVector& _location);
	
private:
	UBehaviorTreeComponent* m_behaviorTreeComponent = nullptr;
	UBlackboardComponent* m_blackboardComponent = nullptr;
	
	//Cache für haufig genutzte Blackboard Keys
	//FBlackboardKeyID m_hungerKeyID;
	//FBlackboardKeyID m_exhaustionKeyID;
	//FBlackboardKeyID m_targetLocationKeyID;
};
