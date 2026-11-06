#include "NPCInteractionTasks.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "EngineUtils.h"
#include "LLM_NPC/NPC/NPCCharacter.h"
#include "LLM_NPC/World/Door.h"
#include "LLM_NPC/Economy/MarketStall.h"

// Kleiner Helfer: den NPC aus dem BT-Owner holen.
static ANPCCharacter* NpcFromOwner(UBehaviorTreeComponent& _ownerComp)
{
	AAIController* aic = _ownerComp.GetAIOwner();
	return aic ? Cast<ANPCCharacter>(aic->GetPawn()) : nullptr;
}

// ============================================================================
//  CONVERSE
// ============================================================================
UBTTask_Converse::UBTTask_Converse()
{
	NodeName    = TEXT("Converse (NPC to NPC)");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_Converse::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8*)
{
	ANPCCharacter* npc = NpcFromOwner(_ownerComp);
	if (!npc) return EBTNodeResult::Failed;

	if (!npc->TryStartNPCConversation())
		return EBTNodeResult::Succeeded;

	return EBTNodeResult::InProgress;
}

void UBTTask_Converse::TickTask(UBehaviorTreeComponent& _ownerComp, uint8*, float)
{
	ANPCCharacter* npc = NpcFromOwner(_ownerComp);
	if (!npc || !npc->IsInConversation())
		FinishLatentTask(_ownerComp, EBTNodeResult::Succeeded);
}

// ============================================================================
//  OPEN DOOR
// ============================================================================
UBTTask_OpenDoor::UBTTask_OpenDoor()
{
	NodeName            = TEXT("Open Door");
	bNotifyTick         = true;
	bCreateNodeInstance = true;   //
}

EBTNodeResult::Type UBTTask_OpenDoor::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8*)
{
	return EBTNodeResult::Succeeded;
}

void UBTTask_OpenDoor::TickTask(UBehaviorTreeComponent& _ownerComp, uint8*, float _dt)
{
	m_waitTimer += _dt;
	if (!m_door.IsValid() || m_door->IsOpen() || m_waitTimer >= MaxWait)
		FinishLatentTask(_ownerComp, EBTNodeResult::Succeeded);
}

// ============================================================================
//  BUY AT MARKET
// ============================================================================
UBTTask_BuyAtMarket::UBTTask_BuyAtMarket()
{
	NodeName = TEXT("Buy at Market");
}

EBTNodeResult::Type UBTTask_BuyAtMarket::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8*)
{
	ANPCCharacter* npc = NpcFromOwner(_ownerComp);
	if (!npc || !npc->GetWorld()) return EBTNodeResult::Failed;

	// Nächsten OFFENEN Stand in Reichweite suchen.
	AMarketStall* stall = nullptr;
	float bestSq = SearchRadius * SearchRadius;
	const FVector from = npc->GetActorLocation();
	for (TActorIterator<AMarketStall> it(npc->GetWorld()); it; ++it)
	{
		AMarketStall* s = *it;
		if (!s || !s->IsOpenNow()) continue;
		if (s->Vendor == npc) continue;   // nicht im eigenen Stand einkaufen
		const float dSq = FVector::DistSquared(from, s->GetActorLocation());
		if (dSq <= bestSq) { bestSq = dSq; stall = s; }
	}
	if (!stall) return EBTNodeResult::Failed;   


	const int32 gold = npc->GetGold();
	for (int32 i = 0; i < stall->Goods.Num(); ++i)
	{
		const int32 price = stall->GetPrice(i);
		if (price >= 0 && price <= gold && stall->Goods[i].Stock != 0)
		{
			if (stall->TryBuyForNPC(npc, i))
				return EBTNodeResult::Succeeded;
		}
	}
	return EBTNodeResult::Failed;   // nichts Bezahlbares vorrätig
}

// ============================================================================
//  WANDER (Nicht-Gras-Punkt, kein Marktplatz-Bezug)
// ============================================================================
UBTTask_Wander::UBTTask_Wander()
{
	NodeName = TEXT("Wander (avoid grass)");
}

EBTNodeResult::Type UBTTask_Wander::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8*)
{
	ANPCCharacter* npc = NpcFromOwner(_ownerComp);
	if (!npc) return EBTNodeResult::Failed;

	FVector target;
	if (!npc->FindWanderTarget(target))
		return EBTNodeResult::Failed;

	if (FNavMeshData* navMesh = npc->GetNavMesh())
	{
		navMesh->SetTargetLocation(target, TEXT("Wander"));
		navMesh->AcceptRadius = 60.0f;
	}
	return EBTNodeResult::Succeeded;
}

// ============================================================================
//  REFLECT
// ============================================================================
UBTTask_Reflect::UBTTask_Reflect()
{
	NodeName = TEXT("Reflect");
}

EBTNodeResult::Type UBTTask_Reflect::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8*)
{
	ANPCCharacter* npc = NpcFromOwner(_ownerComp);
	if (!npc) return EBTNodeResult::Failed;

	const FReflectionData* refl = npc->GetReflection();
	if (!refl || refl->ImportanceSum < MinImportance)
		return EBTNodeResult::Succeeded;   

	npc->TriggerReflection();   // async, best-effort
	return EBTNodeResult::Succeeded;
}

// ============================================================================
//  PLAN DAY
// ============================================================================
UBTTask_PlanDay::UBTTask_PlanDay()
{
	NodeName = TEXT("Plan Day (LLM)");
}

EBTNodeResult::Type UBTTask_PlanDay::ExecuteTask(UBehaviorTreeComponent& /*_ownerComp*/, uint8*)
{
	return EBTNodeResult::Succeeded;
}

// ============================================================================
//  DECIDE GOAL
// ============================================================================
UBTTask_DecideGoal::UBTTask_DecideGoal()
{
	NodeName = TEXT("Decide Goal");
}

EBTNodeResult::Type UBTTask_DecideGoal::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8*)
{
	ANPCCharacter* npc = NpcFromOwner(_ownerComp);
	if (!npc) return EBTNodeResult::Failed;

	npc->ChooseNextDestination();   // nächstes Ziel aus Needs/Plan setzen
	return EBTNodeResult::Succeeded;
}
