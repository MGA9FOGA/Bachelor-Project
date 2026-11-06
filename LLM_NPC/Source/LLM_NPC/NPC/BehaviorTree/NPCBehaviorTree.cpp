#include "NPCBehaviorTree.h"

#include "AIController.h"
#include "LLM_NPC/NPC/NPCCharacter.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "LLM_NPC/World/WorldSpot.h"

// ============================================================================
// BLACKBOARD KEYS INITIALIZATION
// ============================================================================

FName FNPCBlackboardKeys::CurrentHunger		= TEXT("Hunger");
FName FNPCBlackboardKeys::CurrentExhaustion = TEXT("Exhaustion");
FName FNPCBlackboardKeys::DominantEmotion	= TEXT("DominantEmotion");
FName FNPCBlackboardKeys::TopMemory			= TEXT("TopMemory");
FName FNPCBlackboardKeys::TargetLocation	= TEXT("TargetLocation");
FName FNPCBlackboardKeys::CurrentAction		= TEXT("CurrentAction");
FName FNPCBlackboardKeys::IsHungry			= TEXT("IsHungry");
FName FNPCBlackboardKeys::IsExhausted		= TEXT("IsExhausted");
FName FNPCBlackboardKeys::PlayerTalking     = TEXT("PlayerTalking");  


UBTTask_GoToLocation::UBTTask_GoToLocation()
{
	NodeName     = TEXT("Go To Location");
	bNotifyTick  = true;
}

UBTTask_Idle::UBTTask_Idle()
{
	NodeName     = TEXT("Idle");
	bNotifyTick  = true;
}

UBTTask_PerformAction::UBTTask_PerformAction()
{
	NodeName     = TEXT("Perform Action");
	bNotifyTick  = true;
}

UBTTask_Talk::UBTTask_Talk()
{
	NodeName     = TEXT("Talk");
	bNotifyTick  = true;
}

// ============================================================================
// TASK 1: GO TO LOCATION
// ============================================================================

EBTNodeResult::Type UBTTask_GoToLocation::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory)
{
	AAIController* AIC = _ownerComp.GetAIOwner();
	if (!AIC) return EBTNodeResult::Failed;

	ANPCCharacter* npc = Cast<ANPCCharacter>(AIC->GetPawn());
	if (!npc) return EBTNodeResult::Failed;

	UBlackboardComponent* BB = _ownerComp.GetBlackboardComponent();
	if (BB && BB->GetValueAsBool(FNPCBlackboardKeys::PlayerTalking))
		return EBTNodeResult::Failed;

	// Aktionen ohne Ortswechsel → kein MoveTo, direkt zur Aktion.
	// Rest ist NICHT mehr dabei: dafür wird ein Rest-Spot gesucht (oder gewandert),
	// siehe ChooseNextDestination.
	if (FWaypointData* wp = npc->GetWaypoints())
	{
		if (wp->CurrentGoal == ENPCDesire::Idle ||
			wp->CurrentGoal == ENPCDesire::Talk)
		{
			// kein Ortswechsel → Bewegung sauber beenden 
			if (FNavMeshData* nav = npc->GetNavMesh())
				nav->bIsMoving = false;
			return EBTNodeResult::Succeeded; 
		}
	}

	// Das Ziel wird von Decide Goal bzw. Wander gesetzt (sie schreiben
	// navMesh->TargetLocation). 
	FNavMeshData* navMesh = npc->GetNavMesh();

	// Sicherheitsnetz: läuft dieser Zweig ausnahmsweise ohne vorheriges
	// Decide Goal/Wander
	if (navMesh && navMesh->TargetLocation == FVector::ZeroVector)
	{
		npc->ChooseNextDestination();
		navMesh = npc->GetNavMesh();
	}

	if (!navMesh || navMesh->TargetLocation == FVector::ZeroVector)
		return EBTNodeResult::Failed;

	// Ziel aufs NavMesh projizieren – Spot-Offsets/Orts-Z können das Ziel über
	// das NavMesh heben, dann würde MoveTo fehlschlagen. 
	FVector goal = navMesh->TargetLocation;
	bool bProjected = false;
	if (UNavigationSystemV1* navSys = UNavigationSystemV1::GetCurrent(npc->GetWorld()))
	{
		FNavLocation projected;
		if (navSys->ProjectPointToNavigation(goal, projected, FVector(500.0f, 500.0f, 2000.0f)))
		{
			goal = projected.Location;
			bProjected = true;
		}
	}

	FAIMoveRequest moveReq;
	moveReq.SetGoalLocation(goal);
	moveReq.SetAcceptanceRadius(navMesh->AcceptRadius);
	moveReq.SetUsePathfinding(true);
	const FPathFollowingRequestResult moveResult = AIC->MoveTo(moveReq);

	navMesh->bIsMoving      = true;
	navMesh->bTargetReached = false;


	return EBTNodeResult::InProgress;
}

void UBTTask_GoToLocation::TickTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory, float _deltaSeconds)
{
	AAIController* AIC = _ownerComp.GetAIOwner();
	if (!AIC)
	{
		FinishLatentTask(_ownerComp, EBTNodeResult::Failed);
		return;
	}

	ANPCCharacter* npc = Cast<ANPCCharacter>(AIC->GetPawn());
	if (!npc)
	{
		FinishLatentTask(_ownerComp, EBTNodeResult::Failed);
		return;
	}

	// Spieler spricht → sofort stoppen
	UBlackboardComponent* BB = _ownerComp.GetBlackboardComponent();
	if (BB && BB->GetValueAsBool(FNPCBlackboardKeys::PlayerTalking))
	{
		AIC->StopMovement();
		if (FNavMeshData* nav = npc->GetNavMesh())
			nav->bIsMoving = false;
		FinishLatentTask(_ownerComp, EBTNodeResult::Failed);
		return;
	}

	// Angekommen? MoveStatus fragt UE5's Pathfinding
	if (AIC->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		if (FNavMeshData* navMesh = npc->GetNavMesh())
		{
			navMesh->bIsMoving      = false;
			navMesh->bTargetReached = true;
		}
		FinishLatentTask(_ownerComp, EBTNodeResult::Succeeded);
	}
}

FVector UBTTask_GoToLocation::GetTargetLocation(UBehaviorTreeComponent& _owner)
{
	AAIController* AIC = _owner.GetAIOwner();
	if (!AIC) return FVector::ZeroVector;

	ANPCCharacter* npc = Cast<ANPCCharacter>(AIC->GetPawn());
	if (!npc) return FVector::ZeroVector;

	FNavMeshData* navMesh = npc->GetNavMesh();
	return navMesh ? navMesh->TargetLocation : FVector::ZeroVector;
}

bool UBTTask_GoToLocation::HasReachedTarget(ANPCCharacter* _npc)
{
	FNavMeshData* navMesh = _npc ? _npc->GetNavMesh() : nullptr;
	return navMesh ? navMesh->bTargetReached : false;
}

void UBTTask_PerformAction::TickTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory, float _deltaSeconds)
{
	AAIController* AIC = _ownerComp.GetAIOwner();
	if (!AIC) { FinishLatentTask(_ownerComp, EBTNodeResult::Failed); return; }
	ANPCCharacter* npc = Cast<ANPCCharacter>(AIC->GetPawn());
	if (!npc) { FinishLatentTask(_ownerComp, EBTNodeResult::Failed); return; }

	UBlackboardComponent* BB = _ownerComp.GetBlackboardComponent();
	if (BB && BB->GetValueAsBool(FNPCBlackboardKeys::PlayerTalking))
	{
		if (FWaypointData* wp = npc->GetWaypoints())
			wp->bIsPausing = false;
		npc->StopSpotAnimation();           // Gespräch: Loop sofort beenden (kein Outro)
		npc->ReleaseCurrentWorkstation();   // bei Gespräch Platz freigeben
		FinishLatentTask(_ownerComp, EBTNodeResult::Succeeded);
		return;
	}

	FWaypointData* wp = npc->GetWaypoints();
	if (!wp)
	{
		FinishLatentTask(_ownerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Aktion LÄUFT, bis sich das Ziel ändert (Schedule-Stunde oder Needs-Override).
	const UEnum* E = StaticEnum<ENPCDesire>();
	const FString currentGoal = E ? E->GetNameStringByValue((int64)wp->CurrentGoal) : TEXT("");
	FBTStateData* bt = npc->GetBTState();

	if (bt && currentGoal != bt->ActiveTask)
	{
		wp->bIsPausing   = false;
		bt->bTaskRunning = false;
		npc->PlaySpotOutro();               // Sitzen→Stehen (falls am Spot gesetzt), einmalig
		if (npc->IsPlayingOutro())          // Outro läuft → am Spot STEHEN BLEIBEN
			return;                         
		npc->ReleaseCurrentWorkstation();   // Outro fertig → Slot frei + Aktion beenden
		FinishLatentTask(_ownerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Nach kurzer Pause die Aktion beenden, damit der BT zurück zu GoToLocation
	// läuft und neu plant – für:
	//  - Wander: ein neues Zufallsziel (kontinuierliches Schlendern),
	//  - Work OHNE reservierten Spot: erneut auf einen freien Arbeitsplatz prüfen
	//    (Geselle wartet, bis ein Amboss frei wird).
	// "Wartet auf Arbeit" = Ziel Work, aber KEIN echter Arbeitsplatz reserviert
	// (gar keiner, oder nur ein Idle-/Prep-Spot). Dann nach der Pause neu prüfen.
	const AWorldSpot* reserved = npc->GetReservedSpot();
	const bool bWaitingForWork =
		(wp->CurrentGoal == ENPCDesire::Work) &&
		(!reserved || reserved->Type != ESpotType::Work);

	if (wp->CurrentGoal == ENPCDesire::Wander || bWaitingForWork)
	{
		wp->PauseTimer += _deltaSeconds;
		if (wp->PauseTimer >= wp->PauseDuration)
		{
			wp->bIsPausing = false;
			npc->PlaySpotOutro();               // Outro (falls gesetzt), sonst Loop stoppen
			if (npc->IsPlayingOutro())          
				return;
			npc->ReleaseCurrentWorkstation();   // Slot freigeben
			FinishLatentTask(_ownerComp, EBTNodeResult::Succeeded);
		}
	}
}

// ─── UBTTask_Talk ────────────────────────────────────────────
EBTNodeResult::Type UBTTask_Talk::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory)
{
	AAIController* AIC = _ownerComp.GetAIOwner();
	if (!AIC) return EBTNodeResult::Failed;
	ANPCCharacter* npc = Cast<ANPCCharacter>(AIC->GetPawn());
	if (!npc) return EBTNodeResult::Failed;

	AIC->StopMovement();   
	if (FNavMeshData* nav = npc->GetNavMesh())
		nav->bIsMoving = false;

	if (FBTStateData* bt = npc->GetBTState())
	{
		bt->ActiveTask   = TEXT("Talk");   
		bt->bTaskRunning = true;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_Talk::TickTask(UBehaviorTreeComponent& _ownerComp, uint8* _nodeMemory, float _deltaSeconds)
{
	AAIController* AIC = _ownerComp.GetAIOwner();
	if (!AIC) { FinishLatentTask(_ownerComp, EBTNodeResult::Failed); return; }
	ANPCCharacter* npc = Cast<ANPCCharacter>(AIC->GetPawn());
	if (!npc) { FinishLatentTask(_ownerComp, EBTNodeResult::Failed); return; }

	UBlackboardComponent* BB = _ownerComp.GetBlackboardComponent();

	// Gespräch vorbei → Talk-State verlassen, Tagesablauf läuft weiter
	if (!BB || !BB->GetValueAsBool(FNPCBlackboardKeys::PlayerTalking))
	{
		if (FBTStateData* bt = npc->GetBTState())
			bt->bTaskRunning = false;
		FinishLatentTask(_ownerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Zum Spieler drehen (nur Yaw)
	if (AActor* target = npc->GetTalkTarget())
	{
		FVector dir = target->GetActorLocation() - npc->GetActorLocation();
		dir.Z = 0.0f;
		if (!dir.IsNearlyZero())
		{
			const FRotator desired(0.0f, dir.Rotation().Yaw, 0.0f);
			const FRotator smooth = FMath::RInterpTo(npc->GetActorRotation(), desired, _deltaSeconds, TurnSpeed);
			npc->SetActorRotation(smooth);
		}
	}
}

EBTNodeResult::Type UBTTask_Idle::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	IdleTimer    = 0.0f;
	IdleDuration = FMath::RandRange(3.0f, 8.0f);

	if (AAIController* AIC = OwnerComp.GetAIOwner())
	{
		if (ANPCCharacter* npc = Cast<ANPCCharacter>(AIC->GetPawn()))
		{
			if (FBTStateData* btState = npc->GetBTState())
			{
				btState->ActiveTask   = TEXT("Idle");
				btState->bTaskRunning = true;
			}
		}
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_Idle::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (BB && BB->GetValueAsBool(FNPCBlackboardKeys::PlayerTalking))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	IdleTimer += DeltaSeconds;
	if (IdleTimer >= IdleDuration)
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}

// ─── UBTTask_CheckNeeds ──────────────────────────────────────
EBTNodeResult::Type UBTTask_CheckNeeds::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* NodeMemory)
{
	AAIController* AIC = _ownerComp.GetAIOwner();
	if (!AIC) return EBTNodeResult::Succeeded;

	ANPCCharacter* npc = Cast<ANPCCharacter>(AIC->GetPawn());
	if (!npc) return EBTNodeResult::Succeeded;

	UpdateNeedsOnBlackboard(_ownerComp, npc);
	return EBTNodeResult::Succeeded; 
}

void UBTTask_CheckNeeds::UpdateNeedsOnBlackboard(UBehaviorTreeComponent& _ownerComp, ANPCCharacter* _npc)
{
	UBlackboardComponent* BB = _ownerComp.GetBlackboardComponent();
	if (!BB) return;

	FNeedsData* needs = _npc->GetNeeds();
	if (!needs) return;

	// Spiegelt den ECS-Zustand ins Blackboard (für Decorators / Debug)
	BB->SetValueAsBool(FNPCBlackboardKeys::IsHungry,    needs->IsHungry());
	BB->SetValueAsBool(FNPCBlackboardKeys::IsExhausted, needs->IsExhausted());
}

// ─── UBTTask_PerformAction ───────────────────────────────────
EBTNodeResult::Type UBTTask_PerformAction::ExecuteTask(UBehaviorTreeComponent& _ownerComp, uint8* NodeMemory)
{
	AAIController* AIC = _ownerComp.GetAIOwner();
	if (!AIC) return EBTNodeResult::Failed;
	ANPCCharacter* npc = Cast<ANPCCharacter>(AIC->GetPawn());
	if (!npc) return EBTNodeResult::Failed;

	FWaypointData* wp = npc->GetWaypoints();
	FBTStateData*  bt = npc->GetBTState();
	if (!wp || !bt) return EBTNodeResult::Failed;

	const UEnum* E = StaticEnum<ENPCDesire>();
	FString actionName = E ? E->GetNameStringByValue((int64)wp->CurrentGoal) : TEXT("None");
	bt->ActiveTask   = actionName;
	bt->bTaskRunning = true;

	UE_LOG(LogTemp, Warning, TEXT("[Action] %s: %s"), *npc->GetNPCName(), *actionName);

	// Am reservierten Spot: exakt zur Spot-Ausrichtung drehen + Spot-Animation
	// abspielen (Intro→Loop; die Montagen liegen am Spot).
	if (AWorldSpot* spot = npc->GetReservedSpot())
	{

		const FTransform st = spot->GetStandTransformFor(npc->GetEntityID());
		FVector loc = st.GetLocation();
		loc.Z = npc->GetActorLocation().Z;

		const float snapDist2D = FVector::Dist2D(npc->GetActorLocation(), loc);

		bool bWallBetween = false;
		if (snapDist2D > 80.0f)   
		{
			if (UWorld* w = npc->GetWorld())
			{
				const FVector up(0.0f, 0.0f, 40.0f);   
				FHitResult hit;
				FCollisionQueryParams qp;
				qp.AddIgnoredActor(npc);
				qp.AddIgnoredActor(spot);
				bWallBetween = w->LineTraceSingleByChannel(
					hit, npc->GetActorLocation() + up, loc + up, ECC_WorldStatic, qp);
			}
		}

		if (!bWallBetween)
		{
			npc->SetActorLocationAndRotation(loc, st.Rotator());
			npc->PlaySpotAnimation(spot);
		}
		else
		{
			const FVector sl = st.GetLocation();
			const FVector np = npc->GetActorLocation();
			const FVector sc = spot->GetActorScale3D();
		}
	}
	else
	{
		// Kein reservierter AWorldSpot, aber ein Marktstand-Slot (Verkäufer am
		// VendorSlot / Käufer am CustomerSlot)? → dorthin stellen + Blickrichtung.
		npc->TrySnapToStallStand();
	}

	npc->TryConsumePending();

	wp->bIsPausing    = true;
	wp->PauseTimer    = 0.0f;
	wp->PauseDuration = FMath::RandRange(wp->PauseTimeMin, wp->PauseTimeMax);

	return EBTNodeResult::InProgress;
}