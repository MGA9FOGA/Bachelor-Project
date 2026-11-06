#include "LLM_NPC/NPC/Components/NPC_ECS.h"
#include "LLM_NPC/NPC/NPCCharacter.h"
#include "LLM_NPC/World/WorldClock.h"

#include <atomic>

int32 FScheduleSystem::CurrentGameHour = 6;
float FScheduleSystem::GameHoursPerSecond = 1.0f / 30.0f; 

// ============================================================================
// SYSTEMS IMPLEMENTATION
// ============================================================================

void FMemoryDecaySystem::Update(FNPCEntity& _entity, float _deltaTime)
{
	//Decay Recency Aller Memories
	const float DECAY_RATE = 0.99f;

	for (FMemoryEntry& memory : _entity.Memory.Memories)
	{
		memory.Recency *= DECAY_RATE;
	}
}

void FEmotionDecaySystem::Update(FNPCEntity& _entity, float _deltaTime)
{
	//Decay Intensity aller aktiven Emotionen
	const float DECAY_RATE = 0.99f;

	for (int32 i = _entity.Emotion.ActiveEmotions.Num() -1; i >= 0; --i)
	{
		_entity.Emotion.ActiveEmotions[i].Itensity *= DECAY_RATE;

		//Entferne wenn unter Threshold
		if (_entity.Emotion.ActiveEmotions[i].Itensity < 0.01f)
		{
			_entity.Emotion.ActiveEmotions.RemoveAt(i);
		}
	}
}

void FTransformSyncSystem::Update(FNPCEntity& _entity, float _deltaTime)
{
	//Sync Transform vom UE5 Actor
	if (_entity.Transform.OwnerActor)
	{
		_entity.Transform.Location = _entity.Transform.OwnerActor->GetActorLocation();
		_entity.Transform.Rotation = _entity.Transform.OwnerActor->GetActorRotation();
	}
}

void FNeedsUpdateSystem::Update(FNPCEntity& _entity, float _deltaTime)
{
	FNeedsData&      N       = _entity.Needs;
	const ENPCDesire goal    = _entity.Waypoint.CurrentGoal;
	const bool       bMoving = _entity.NavMesh.bIsMoving;     

	// Aktion am Ort zählt erst, wenn die loopende Tätigkeits-Animation läuft 
	const bool       bAtSpot = _entity.Waypoint.bIsPausing && _entity.Waypoint.bLoopActive;

	const bool bHasSpot = _entity.Waypoint.bHasSpot;   

	const float gh = _deltaTime * FScheduleSystem::GameHoursPerSecond;

	const bool bRestWandering = (goal == ENPCDesire::Rest && !(bHasSpot && bAtSpot));

	// 1) Aktiv (laufen ODER an einem ECHTEN Arbeitsplatz arbeiten) → Erschöpfung +
	//    Hunger steigen.
	const bool bActive =
		(bMoving || (bAtSpot && goal == ENPCDesire::Work && bHasSpot)) && !bRestWandering;
	if (bActive)
	{
		N.Exhaustion += N.ExhaustionRate * gh;
		N.Hunger     += N.HungerRate     * gh;
	}

	// 2) Erholung
	if (bAtSpot && goal == ENPCDesire::Eat)
	{
		N.Hunger     -= N.EatRecoveryRate     * gh;   // sättigt
		N.Exhaustion -= N.EatExhaustionRelief * gh;   // erholt zusätzlich leicht
	}
	else if (bAtSpot && goal == ENPCDesire::Drink)
	{
		N.Exhaustion -= N.DrinkRefreshRate * gh;       // erfrischt leicht (kein Sättigen)
	}
	else if (bAtSpot && goal == ENPCDesire::Sleep)
	{
		N.Exhaustion -= N.SleepRecoveryRate * gh;     // Haupterholung im Bett
		N.Hunger     += N.SleepHungerRate   * gh;     // im Schlaf langsam hungrig → morgens essen
	}
	else if (goal == ENPCDesire::Rest)
	{
		if (bHasSpot)
		{
			if (bAtSpot) N.Exhaustion -= N.RestRecoveryRate * gh;
		}
		else
		{.
			N.Exhaustion -= N.WanderRestRecoveryRate * gh;
		}
	}

	// 3) Sonst (Idle / Talk / unterwegs) → Needs bleiben gleich
	N.Hunger     = FMath::Clamp(N.Hunger,     0.0f, 1.0f);
	N.Exhaustion = FMath::Clamp(N.Exhaustion, 0.0f, 1.0f);
}

void FWanderSystem::Update(FNPCEntity& _entity, float _deltaTime)
{
	if (_entity.Waypoint.CurrentGoal != ENPCDesire::Wander)
		return;
	
	_entity.Waypoint.bWantsRandomLocation = true;
}

void FWanderSystem::DecideNextGoal(FNPCEntity& _entity) const
{
	// Bedürfnisgetriebene Priorität:
	if (_entity.Needs.IsHungry())
	{
		_entity.Waypoint.CurrentGoal = ENPCDesire::Eat;
	}
	else if (_entity.Needs.IsExhausted())
	{
		_entity.Waypoint.CurrentGoal = ENPCDesire::Sleep;
	}
	else
	{
		// Keine dringenden Needs → arbeiten (Beruf) oder umherziehen
		// 70% arbeiten, 30% einfach wandern
		_entity.Waypoint.CurrentGoal =
			(FMath::RandRange(0, 100) < 70) ? ENPCDesire::Work : ENPCDesire::Wander;
	}
}

void FScheduleSystem::Update(FNPCEntity& _entity, float _deltaTime)
{
	if (!_entity.Schedule.bScheduleReady ||
		_entity.Schedule.DailySchedule.Num() == 0)
		return;

	// Verkäufer mit GEÖFFNETEM Stand bleibt IMMER am Posten: HIER (an der Quelle des
	// Ziels) auf Work zwingen und Plan/Needs-Override überspringen. 
	if (_entity.Schedule.bVendorOnDuty)
	{
		if (_entity.Waypoint.CurrentGoal != ENPCDesire::Work)
		{
			_entity.Waypoint.CurrentGoal = ENPCDesire::Work;
		}

		_entity.Schedule.CurrentActivityIndex = -1;
		return;
	}


	// 2. Needs-Override (Hysterese). Den Schlaf-/Wach-Rhythmus plant das LLM selbst
	//    (sleep-Einträge im Tagesplan). Der Schlaf-Typ liefert nur einen HINWEIS,
	//    ab wann "Schlafenszeit" ist – kein erzwungenes Nachtfenster mehr.
	FNeedsData& N = _entity.Needs;

	// "Schlafenszeit nah?" = im typgerechten Schlaffenster, mit kurzem Vorlauf, damit
	// ein müder NPC kurz vor dem Zubettgehen nicht mehr ausruht, sondern direkt
	// schlafen geht.
	constexpr int32 preBedHours = 2;
	const int32 bedStart = (N.SleepHourThreshold + 24 - preBedHours) % 24;
	const bool bNearBedtime = (bedStart < N.WakeHourThreshold)
		? (CurrentGameHour >= bedStart && CurrentGameHour < N.WakeHourThreshold)
		: (CurrentGameHour >= bedStart || CurrentGameHour <  N.WakeHourThreshold);

	// Essen hat Vorrang – aber nicht zur Schlafenszeit 
	if (!bNearBedtime && N.IsHungry())
		N.OverrideState = ENeedOverride::Eat;
	else if (N.OverrideState == ENeedOverride::Eat && N.Hunger <= N.HungerSatisfied)
		N.OverrideState = ENeedOverride::None;

	if (N.OverrideState != ENeedOverride::Eat)
	{
		const bool bRecovering =
			(N.OverrideState == ENeedOverride::Rest || N.OverrideState == ENeedOverride::Sleep);

		if (N.IsExhausted() && !bRecovering)
		{
			N.OverrideState = bNearBedtime ? ENeedOverride::Sleep : ENeedOverride::Rest;
		}
		else if (bRecovering && N.Exhaustion <= N.ExhaustionSatisfied)
		{
			N.OverrideState = ENeedOverride::None;  
	}


	if (N.OverrideState == ENeedOverride::Eat)
	{
		_entity.Waypoint.CurrentGoal = ENPCDesire::Eat;
		_entity.Schedule.CurrentActivityIndex = -1;
		return;
	}
	if (N.OverrideState == ENeedOverride::Rest)
	{
		_entity.Waypoint.CurrentGoal = ENPCDesire::Rest;
		_entity.Schedule.CurrentActivityIndex = -1;
		return;
	}
	if (N.OverrideState == ENeedOverride::Sleep)
	{
		_entity.Waypoint.CurrentGoal = ENPCDesire::Sleep;
		_entity.Schedule.CurrentActivityIndex = -1;
		return;
	}


	// ===== PLAN — passenden Eintrag zur aktuellen Stunde finden =====
	// Suche den letzten Eintrag dessen Stunde <= jetzt ist
	int32 bestIndex = -1;
	int32 bestHour  = -1;

	for (int32 i = 0; i < _entity.Schedule.DailySchedule.Num(); i++)
	{
		int32 entryHour = _entity.Schedule.DailySchedule[i].Hour;
		if (entryHour <= CurrentGameHour && entryHour > bestHour)
		{
			bestHour  = entryHour;
			bestIndex = i;
		}
	}

	// Nichts gefunden (z.B. früh morgens vor erstem Eintrag) → letzter Eintrag des Vortags
	if (bestIndex == -1)
		bestIndex = _entity.Schedule.DailySchedule.Num() - 1;

	// Nur bei Wechsel aktualisieren
	if (bestIndex != _entity.Schedule.CurrentActivityIndex)
	{
		_entity.Schedule.CurrentActivityIndex = bestIndex;
		_entity.Schedule.CurrentActivity = _entity.Schedule.DailySchedule[bestIndex];

		FString activity = _entity.Schedule.CurrentActivity.Activity;
		_entity.Waypoint.CurrentGoal = ActivityStringToDesire(activity);
		UE_LOG(LogTemp, Warning, TEXT("[Schedule] Stunde %d -> %s"), CurrentGameHour, *activity);
	}
}

ENPCDesire FScheduleSystem::ActivityStringToDesire(const FString& _activity) const
{
	if (_activity.Equals(TEXT("eat"),    ESearchCase::IgnoreCase)) return ENPCDesire::Eat;
	if (_activity.Equals(TEXT("drink"),  ESearchCase::IgnoreCase)) return ENPCDesire::Drink;
	if (_activity.Equals(TEXT("sleep"),  ESearchCase::IgnoreCase)) return ENPCDesire::Sleep;
	if (_activity.Equals(TEXT("work"),   ESearchCase::IgnoreCase)) return ENPCDesire::Work;
	if (_activity.Equals(TEXT("wander"), ESearchCase::IgnoreCase)) return ENPCDesire::Wander;
	if (_activity.Equals(TEXT("idle"),   ESearchCase::IgnoreCase)) return ENPCDesire::Idle;
	if (_activity.Equals(TEXT("rest"), ESearchCase::IgnoreCase)) return ENPCDesire::Rest;
	if (_activity.Equals(TEXT("shop"), ESearchCase::IgnoreCase)) return ENPCDesire::Shop;
	return ENPCDesire::None;
}

// ============================================================================
// MAIN ECS MANAGER IMPLEMENTATION
// ============================================================================

FNPCECS::FNPCECS()
{
	InitalizeSystems();
}

FNPCECS::~FNPCECS()
{
	m_entities.Empty();
}

EntityID FNPCECS::CreateEntity(ANPCCharacter* _owner)
{
	// Global eindeutige IDs über ALLE NPC-ECS-Instanzen hinweg.
	// Jeder NPC hält sein eigenes FNPCECS; ein per-Instanz-Zähler würde
	// jedem NPC dieselbe ID 1 geben und globale Systeme (Workstation-
	// Reservierung im WorldLocationManager, Beziehungen) NPCs verwechseln
	// lassen.
	static std::atomic<EntityID> GNextGlobalEntityID{ 1 };
	const EntityID newID = GNextGlobalEntityID.fetch_add(1);

	FNPCEntity newEntity(newID, _owner);
	m_entities.Add(newID, newEntity);

	return newID;
}

void FNPCECS::DestroyEntity(EntityID _id)
{
	if (m_entities.Contains(_id))
	{
		m_entities.Remove(_id);
	}
}

FNPCEntity* FNPCECS::GetEntity(EntityID _id)
{
	return m_entities.Find(_id);
}

const FNPCEntity* FNPCECS::GetEntity(EntityID _id) const
{
	return m_entities.Find(_id);
}

void FNPCECS::Update(float _deltaTime)
{
	//Aktualisiere alle Entities mit allen Systems
	for (auto& pair : m_entities)
	{
		FNPCEntity& entity = pair.Value;

		for (auto& system : Systems)
		{
			if (!system) continue;

			int32 sysFlag = (int32)system->GetSystemType();
			if ((entity.ActiveSystemFlags & sysFlag) != 0)
			{
				system->Update(entity, _deltaTime);
			}
		}
	}
}

void FNPCECS::DebugPrintEntity(EntityID _id) const
{
	const FNPCEntity* entity = GetEntity(_id);
	if (!entity)
	{
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("=== ECS ENTITY DEBUG: %d ==="), _id);
	
	// Identity
	UE_LOG(LogTemp, Warning, TEXT("Identity: %s (%s)"), *entity->Identity.Name, *entity->Identity.Role);
	
	// Memory
	UE_LOG(LogTemp, Warning, TEXT("Memory: %d entries"), entity->Memory.Memories.Num());
	for (const FMemoryEntry& Mem : entity->Memory.GetTopMemories(3))
	{
		UE_LOG(LogTemp, Warning, TEXT("  - %s (Score: %.2f)"), 
			*Mem.Content, Mem.GetRetrievalScore());
	}
	
	// Emotion
	UE_LOG(LogTemp, Warning, TEXT("Emotion: %d active"), entity->Emotion.ActiveEmotions.Num());
	
	// Transform
	UE_LOG(LogTemp, Warning, TEXT("Transform: (%.0f, %.0f, %.0f)"), 
		entity->Transform.Location.X, entity->Transform.Location.Y, entity->Transform.Location.Z);
	
	// NavMesh
	UE_LOG(LogTemp, Warning, TEXT("NavMesh: Moving=%s, Target=%s"), 
		entity->NavMesh.bIsMoving ? TEXT("YES") : TEXT("NO"), *entity->NavMesh.TargetTag);
	
	// BTState
	UE_LOG(LogTemp, Warning, TEXT("BTState: %s (Running=%s)"), 
		*entity->BTState.ActiveTask, entity->BTState.bTaskRunning ? TEXT("YES") : TEXT("NO"));
	
	// Needs
	UE_LOG(LogTemp, Warning, TEXT("Needs: Hunger=%.2f, Exhaustion=%.2f"),
		entity->Needs.Hunger, entity->Needs.Exhaustion);
	
	// Waypoints
	//UE_LOG(LogTemp, Warning, TEXT("Waypoints: %d (Current=%d, Pausing=%s)"), 
	//	entity->Waypoint.Waypoints.Num(), entity->Waypoint.CurrentWaypointIndex,
	//	entity->Waypoint.bIsPausing ? TEXT("YES") : TEXT("NO"));
}


void FNPCECS::InitalizeSystems()
{
	// Erstelle alle Systems in Execution-Order
	Systems.Add(MakeUnique<FMemoryDecaySystem>());
	Systems.Add(MakeUnique<FEmotionDecaySystem>());
	Systems.Add(MakeUnique<FTransformSyncSystem>());
	Systems.Add(MakeUnique<FNeedsUpdateSystem>());
	Systems.Add(MakeUnique<FScheduleSystem>()); 
	Systems.Add(MakeUnique<FWanderSystem>());
}






