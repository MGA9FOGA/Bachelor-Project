#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "NPCWorldEnums.h"
#include "LLM_NPC/Economy/InventoryData.h"
#include "LLM_NPC/World/WorldLocationDataAsset.h"


// Forward Declarations
class ANPCCharacter;
struct FNPCEntity;

/**
 * ============================================================================
 * EIGENSTÄNDIGES ECS SYSTEM FÜR NPC
 * ============================================================================
 *
 * Dies ist KEIN UE5-Component-System, sondern ein reines Daten-Driven ECS:
 * - Entity: Hat eine ID, hält alle Komponenten
 * - Components: Reine Datenstrukturen (POD)
 * - Systems: Verarbeiten Komponenten-Daten
 *
 * Der Character HÄLT dieses ECS-System und updatet es in seinem Tick.
 */

// ============================================================================
// ENTITY ID
// ============================================================================

using EntityID = uint32;
constexpr EntityID INVALID_ENTITY = 0;

// ============================================================================
// KOMPONENTEN-TYPEN (Pure Data Structs)
// ============================================================================

// === GRUPPE A: LLM-DATA ===

struct FIdentityData
{
	FString Name;
	FString Role;
	FString Personality;
	TArray<FString> Goals;
	
	EWorldLocationTag HomeTag = EWorldLocationTag::None;
	EWorldLocationTag WorkTag = EWorldLocationTag::None;
};

struct FMemoryEntry
{
	FString Content;
	float Importance = 0.5f;
	float Recency = 1.0f;
	float CreationTime = 0.0f;

	float GetRetrievalScore() const
	{
		// Einfaches Beispiel: Je wichtiger und aktueller, desto höher der Score
		return Importance * Recency;
	}
};

struct FMemoryData
{
	static const int32 MAX_MEMORIES = 50;
	TArray<FMemoryEntry> Memories;

	void AddMemory(const FString& _content, float _importance = 0.5f, float _currentTime = 0.0f)
	{
		FMemoryEntry entry;
		entry.Content = _content;
		entry.Importance = FMath::Clamp(_importance, 0.0f, 1.0f);
		entry.Recency = 1.0f; // Neueste Erinnerung
		entry.CreationTime = _currentTime;

		Memories.Add(entry);

		// Prune wenn zu viele
		if (Memories.Num() > MAX_MEMORIES)
		{
			// Entferne Memory mit niedrigstem Score
			int32 lowestIDX = 0;
			float lowestScore = Memories[0].GetRetrievalScore();

			for (int32 i = 1; i < Memories.Num(); i++)
			{
				float score = Memories[i].GetRetrievalScore();
				if (score < lowestScore)
				{
					lowestScore = score;
					lowestIDX = i;
				}
			}

			Memories.RemoveAt(lowestIDX);
		}
	}

	TArray<FMemoryEntry> GetTopMemories(int32 _count = 5) const
	{
		TArray<FMemoryEntry> sorted = Memories;
		sorted.Sort([](const FMemoryEntry& A, const FMemoryEntry& B)
		{
			return A.GetRetrievalScore() > B.GetRetrievalScore(); // Absteigend sortieren
		});

		TArray<FMemoryEntry> topMemories;
		for (int32 i = 0; i < FMath::Min(_count, sorted.Num()); i++)
		{
			topMemories.Add(sorted[i]);
		}

		return topMemories;
	}
};

UENUM(BlueprintType)
enum class EEmotionType : uint8
{
	Neutral   UMETA(DisplayName = "Neutral"),
	Happy     UMETA(DisplayName = "Happy"),
	Suspicious UMETA(DisplayName = "Suspicious"),
	Concerned UMETA(DisplayName = "Concerned"),
	Angry     UMETA(DisplayName = "Angry")
};
UENUM(BlueprintType)
enum class EIntentType : uint8
{
	Neutral  UMETA(DisplayName = "Neutral"),
	Greeting UMETA(DisplayName = "Greeting"),
	Trade    UMETA(DisplayName = "Trade"),
	Hostile  UMETA(DisplayName = "Hostile"),
	Quest    UMETA(DisplayName = "Quest")
};


static FString IntentToString(EIntentType _intent)
{
	switch (_intent)
	{
	case EIntentType::Greeting: return TEXT("greeting");
	case EIntentType::Trade:    return TEXT("trade");
	case EIntentType::Hostile:  return TEXT("hostile");
	case EIntentType::Quest:    return TEXT("quest");
	default:                    return TEXT("neutral");
	}
}

enum class ESystemType : uint8
{
	MemoryDecay  = 1 << 0,
	EmotionDecay = 1 << 1,
	Needs        = 1 << 2,
	Wander       = 1 << 3,
	TransformSync= 1 << 4,
	Schedule	 = 1 << 5
};

static EIntentType StringToIntent(const FString& _intent)
{
	if (_intent == TEXT("greeting")) return EIntentType::Greeting;
	if (_intent == TEXT("trade"))    return EIntentType::Trade;
	if (_intent == TEXT("hostile"))  return EIntentType::Hostile;
	if (_intent == TEXT("quest"))    return EIntentType::Quest;
	return EIntentType::Neutral;
}

static FString EmotionToMoodString(EEmotionType _mood)
{
	switch (_mood)
	{
	case EEmotionType::Happy:      return TEXT("friendly");
	case EEmotionType::Suspicious: return TEXT("suspicious");
	case EEmotionType::Angry:      return TEXT("angry");
	case EEmotionType::Concerned:  return TEXT("suspicious");
	default:                       return TEXT("neutral");
	}
}

static EEmotionType MoodStringToEmotion(const FString& _mood)
{
	if (_mood == TEXT("friendly"))   return EEmotionType::Happy;
	if (_mood == TEXT("suspicious")) return EEmotionType::Suspicious;
	if (_mood == TEXT("angry"))      return EEmotionType::Angry;
	return EEmotionType::Neutral;
}

struct FEmotionState
{
	EEmotionType type = EEmotionType::Neutral;
	float Itensity = 0.0f; // 0.0 - 1.0
	float StartTime = 0.0f;
};

struct FEmotionData
{
	TArray<FEmotionState> ActiveEmotions;

	void TriggerEmotion(EEmotionType _type, float _intensity, float _currentTime)
	{
		for (FEmotionState& emotion : ActiveEmotions)
		{
			if (emotion.type == _type)
			{
				emotion.Itensity = FMath::Max(emotion.Itensity, FMath::Clamp(_intensity, 0.0f, 1.0f));
				return;
			}
		}

		FEmotionState state;
		state.type = _type;
		state.Itensity = FMath::Clamp(_intensity, 0.0f, 1.0f);
		state.StartTime = _currentTime;
		ActiveEmotions.Add(state);
	}

	EEmotionType GetDominantEmotion() const
	{
		float maxIntensity = 0.0f;
		EEmotionType dominant = EEmotionType::Neutral;

		for (const FEmotionState& emotion : ActiveEmotions)
		{
			if (emotion.Itensity > maxIntensity)
			{
				maxIntensity = emotion.Itensity;
				dominant = emotion.type;
			}
		}

		return dominant;
	}

	float GetDominantIntensity() const
	{
		float maxIntensity = 0.0f;
		for (const FEmotionState& emotion : ActiveEmotions)
		{
			maxIntensity = FMath::Max(maxIntensity, emotion.Itensity);
		}
		return maxIntensity;
	}
};

// Reflexion nach Park et al. (2023): Der NPC sammelt die Wichtigkeit seiner jüngsten
// Ereignisse an; überschreitet sie einen Schwellwert, zieht das Gehirn per LLM höhere
// Einsichten aus den letzten Erinnerungen und schreibt sie als hochgewichtige
// Erinnerungen zurück (→ beeinflussen künftige Entscheidungen).
struct FReflectionData
{
	// Schwelle der angesammelten Wichtigkeit, ab der reflektiert wird.
	// Höher = seltener Reflexion (weniger LLM-Last).
	static constexpr float ReflectionThreshold = 6.0f;

	TArray<FString> Reflections;        // Bisher gebildete Einsichten (Historie)
	float ImportanceSum      = 0.0f;    // Angesammelte Wichtigkeit seit der letzten Reflexion
	float LastReflectionTime = 0.0f;
	bool  bRequestInFlight   = false;   // Läuft gerade eine Reflexions-Anfrage? (kein Doppel)

	// Wichtigkeit eines neuen Ereignisses ansammeln. Gibt true zurück, sobald der
	// Schwellwert erreicht ist (Zeit für eine Reflexion).
	bool Accumulate(float _importance)
	{
		ImportanceSum += FMath::Max(0.0f, _importance);
		return ImportanceSum >= ReflectionThreshold;
	}

	// Nach einer erfolgreichen Reflexion: Zähler zurücksetzen.
	void ResetAccumulation(float _time)
	{
		ImportanceSum      = 0.0f;
		LastReflectionTime = _time;
	}
};

struct FNPCRelation
{
	// Wie viele verdichtete Gesprächs-Erinnerungen pro Person behalten werden – und
	// zugleich, nach wie vielen neuen Erinnerungen eine Meinung neu gebildet wird.
	// Höher = seltener Opinion-LLM-Calls (weniger Last).
	static const int32 MAX_REL_MEMORIES = 5;

	EntityID OtherEntityID = INVALID_ENTITY;
	FString  OtherName;      // Name des anderen NPCs (für lesbare Prompts)
	float Trust = 0.5f;
	float Liking = 0.5f;
	float SocialUrge = 0.0f; // Drang, mit der anderen Entität zu interagieren
	float LastInteractionTime = 0.0f;

	// Verdichtete Erinnerungen an Gespräche mit dieser Person (je ein kurzer Satz,
	// vom LLM destilliert). Das ist die „Beziehungs-Erinnerung": nicht der Wortlaut,
	// sondern was hängen geblieben ist. Älteste zuerst raus (MAX_REL_MEMORIES).
	TArray<FString> Memories;

	// Zweite Verdichtungsstufe: die stabile MEINUNG über diese Person – aus den
	// Erinnerungen periodisch destilliert (ein Satz: Charakter + wie man zueinander steht).
	FString Opinion;

	// Neue Erinnerungen seit der letzten Meinungs-Bildung (Trigger-Zähler).
	int32 MemoriesSinceOpinion = 0;
};

struct FRelationshipData
{
	TArray<FNPCRelation> Relations;

	void SetRelation(EntityID _otherID, float _trust, float _liking)
	{
		FNPCRelation& r = GetOrAddRelation(_otherID);
		r.Trust  = FMath::Clamp(_trust,  0.0f, 1.0f);
		r.Liking = FMath::Clamp(_liking, 0.0f, 1.0f);
	}

	// Nur lesen (kein Anlegen) – null, wenn noch keine Beziehung besteht.
	const FNPCRelation* FindRelation(EntityID _otherID) const
	{
		for (const FNPCRelation& r : Relations)
			if (r.OtherEntityID == _otherID)
				return &r;
		return nullptr;
	}

	// Bestehende Beziehung holen oder neu anlegen.
	FNPCRelation& GetOrAddRelation(EntityID _otherID)
	{
		for (FNPCRelation& relation : Relations)
			if (relation.OtherEntityID == _otherID)
				return relation;

		FNPCRelation newRelation;
		newRelation.OtherEntityID = _otherID;
		Relations.Add(newRelation);
		return Relations.Last();
	}

	// Ein Gespräch verändert Sympathie/Vertrauen. Die Deltas können NEGATIV sein
	// (wenn zwei sich nicht mögen) → so entstehen auch Abneigungen, nicht nur
	// Freundschaften. Clamp hält alles in [0,1].
	void AdjustRelation(EntityID _otherID, const FString& _otherName,
	                    float _likingDelta, float _trustDelta, float _time)
	{
		FNPCRelation& r = GetOrAddRelation(_otherID);
		if (!_otherName.IsEmpty()) r.OtherName = _otherName;
		r.Liking = FMath::Clamp(r.Liking + _likingDelta, 0.0f, 1.0f);
		r.Trust  = FMath::Clamp(r.Trust  + _trustDelta,  0.0f, 1.0f);
		r.LastInteractionTime = _time;
	}

	// Eine verdichtete Gesprächs-Erinnerung zu dieser Person ablegen (älteste zuerst
	// raus). Das LLM liefert den kurzen Satz nach jedem Gespräch.
	void AddMemory(EntityID _otherID, const FString& _otherName, const FString& _memory)
	{
		if (_memory.IsEmpty()) return;
		FNPCRelation& r = GetOrAddRelation(_otherID);
		if (!_otherName.IsEmpty()) r.OtherName = _otherName;
		r.Memories.Add(_memory);
		while (r.Memories.Num() > FNPCRelation::MAX_REL_MEMORIES)
			r.Memories.RemoveAt(0);
		r.MemoriesSinceOpinion++;
	}

	// Beziehungen driften langsam Richtung neutral (0.5), wenn sie nicht durch
	// Gespräche aufgefrischt werden – Freundschaften UND Abneigungen verblassen.
	void DecayTowardNeutral(float _step)
	{
		if (_step <= 0.0f) return;
		auto drift = [](float _v, float _s)
		{
			return (_v > 0.5f) ? FMath::Max(0.5f, _v - _s) : FMath::Min(0.5f, _v + _s);
		};
		for (FNPCRelation& r : Relations)
		{
			r.Liking = drift(r.Liking, _step);
			r.Trust  = drift(r.Trust,  _step);
		}
	}
};

struct FDayPlaynEntry
{
	int32 Hour = 0;
	FString Activity;
	FVector Location = FVector::ZeroVector;
};

struct FScheduleData
{
	TArray<FDayPlaynEntry> DailySchedule;
	FDayPlaynEntry CurrentActivity;
	int32 CurrentActivityIndex = -1;

	bool bScheduleReady = false;

	// Verkäufer mit GEÖFFNETEM eigenen Stand? Vom Character pro Frame gesetzt
	// (vor dem ECS-Update). Solange true, erzwingt der Schedule-System selbst
	// "Work" und überspringt Plan/Needs-Override – der Verkäufer bleibt am Posten.
	// So ist der Schedule die EINZIGE Instanz, die CurrentGoal schreibt; früher
	// überschrieb ChooseNextDestination das Ziel und beide fochten gegeneinander
	// (Work↔Shop/Eat-Pingpong → BT-Abbrüche → LLM-Neuplanungs-Spam).
	bool bVendorOnDuty = false;
};

struct FDialogTurn
{
	bool bIsNPCResponse = false;
	FString DialogText;
	float Timestamp = 0.0f;
};

struct FDialogData
{
	TArray<FDialogTurn> DialogHistory;
	bool bIsInDialog = false;
	float LastDialogInputTime = 0.0f;

	static const int32 MAX_DIALOG_HISTORY = 10;
};

// Laufzeit-Zustand eines NPC↔NPC-Gesprächs (wer, ob aktiv, Dialogverlauf).
struct FConversationData
{
	static const int32 MAX_TURNS = 10;

	EntityID PartnerEntityID = INVALID_ENTITY;
	TArray<FString> ConversationHistory;
	bool  bIsActive    = false;
	float LastTurnTime = 0.0f;

	// Gespräch mit _partner beginnen (Verlauf zurücksetzen).
	void Begin(EntityID _partner, float _time)
	{
		PartnerEntityID = _partner;
		bIsActive       = true;
		LastTurnTime    = _time;
		ConversationHistory.Reset();
	}

	// Gespräch beenden (Verlauf bleibt für ggf. Auswertung stehen).
	void End()
	{
		bIsActive       = false;
		PartnerEntityID = INVALID_ENTITY;
	}

	// Eine Dialogzeile anhängen (auf MAX_TURNS begrenzt, älteste zuerst raus).
	// Dient dem Spieler-Dialog ("gerade geredet mit …"); die Langzeit-Kontinuität
	// läuft über verdichtete Beziehungs-Erinnerungen (FNPCRelation::Memories).
	void AddTurn(const FString& _line, float _time)
	{
		if (_line.IsEmpty()) return;
		ConversationHistory.Add(_line);
		LastTurnTime = _time;
		while (ConversationHistory.Num() > MAX_TURNS)
			ConversationHistory.RemoveAt(0);
	}
};

// === GRUPPE B: EXECUTION ===

struct FTransformData
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	AActor* OwnerActor = nullptr; // Referenz zum Actor, um Transform-Updates zu ermöglichen
};

struct FNavMeshData
{
	FVector TargetLocation = FVector::ZeroVector;
	FString TargetTag;
	bool bIsMoving = false;
	bool bTargetReached = false;
	float AcceptRadius = 50.0f;
	
	void SetTargetLocation(const FVector& _targetLocation, const FString& _targetTag = TEXT(""))
	{
		TargetLocation = _targetLocation;
		TargetTag = _targetTag;
		bIsMoving = false;
		bTargetReached = false;
	}
	
	void ClearTarget()
	{
		TargetLocation = FVector::ZeroVector;
		TargetTag = TEXT("");
		bIsMoving = false;
		bTargetReached = false;
	}
};

// ENPCDesire ist jetzt zentral in NPCWorldEnums.h definiert (von NPC UND World
// genutzt, z.B. als Schlüssel für aktionsabhängige Spot-Animationen).

enum class EBTTaskResult : uint8
{
	Succeeded,
	Failed,
	InProgress,
	Aborted
};

struct FBTStateData
{
	FString ActiveTask = TEXT("Idle");
	EBTTaskResult TaskResult = EBTTaskResult::Succeeded;
	bool bTaskRunning = false;
	float TaskStartTime = 0.0f;
	float TaskDuration = 0.0f;
};

// === GRUPPE C: EXTENSION ===

// Aktiver, über die Schwelle ausgelöster Needs-Override (Hysterese-Zustand).
// Rest und Sleep sind getrennt, damit die Wahl EINMAL fällt und nicht kippt.
enum class ENeedOverride : uint8 { None = 0, Eat = 1, Rest = 2, Sleep = 3 };

struct FNeedsData
{
	float Hunger = 0.3f; // 0.0 - 1.0
	float Exhaustion = 0.2f; // 0.0 - 1.0
	float LastMealTime = 0.0f;

	float HungerThreshhold = 0.6f;
	float ExhaustionThreshhold = 0.7f;

	// "Satisfied"-Schwellen für Hysterese: einmal angefangen, wird gegessen/geruht,
	// bis der Wert SO WEIT unten ist (verhindert Thrashing am Auslöse-Schwellwert).
	float HungerSatisfied     = 0.15f;
	float ExhaustionSatisfied = 0.20f;

	// Welcher Bedarf erzwingt gerade das Ziel (nur wenn echt über die Schwelle ausgelöst)
	ENeedOverride OverrideState = ENeedOverride::None;
	
	// Anstieg pro SPIELSTUNDE, während der NPC aktiv ist (läuft/arbeitet).
	// Default grob auf ~2x essen / ~1-2x ausruhen pro Tag getunt.
	float HungerRate     = 0.05f;   // langsamer (vorher 0.09 → wurde zu oft hungrig)
	float ExhaustionRate = 0.07f;

	// Erholung pro Spielstunde am Zielort
	float EatRecoveryRate   = 3.0f;   // essen (Hunger runter)
	float RestRecoveryRate  = 0.5f;   // ausruhen am Rest-Spot – bewusst langsam, damit
	                                  // der NPC deutlich länger sitzt (vorher 2.0 = zu kurz)
	float SleepRecoveryRate = 2.5f;   // schlafen (nachts, Haupterholung)
	float EatExhaustionRelief = 0.5f; // Essen wirkt zusätzlich LEICHT der Erschöpfung entgegen
	float DrinkRefreshRate    = 0.3f; // Trinken erfrischt leicht (Erschöpfung runter)

	// Erholung beim Umherwandern, wenn KEIN Rest-Spot frei war – MUSS langsamer als
	// RestRecoveryRate sein (Bank besser als Schlendern).
	float WanderRestRecoveryRate = 0.2f;

	// Hunger steigt auch im Schlaf LEICHT (Stoffwechsel) → morgens erstmal essen.
	float SleepHungerRate = 0.04f;

	// Nachtfenster für Schlaf-Override: Nacht = Stunde >= SleepHour ODER < WakeHour.
	int32 SleepHourThreshold = 20;   // ab hier (abends) ist Nacht
	int32 WakeHourThreshold  = 6;    // bis hier (morgens) ist noch Nacht

	bool IsHungry() const { return Hunger >= HungerThreshhold; }
	bool IsExhausted() const { return Exhaustion >= ExhaustionThreshhold; }
};

struct FWaypointData
{
	bool  bIsPausing    = false;
	float PauseTimer    = 0.0f;
	float PauseDuration = 0.0f;
	float PauseTimeMin  = 3.0f;
	float PauseTimeMax  = 8.0f;
	
	ENPCDesire CurrentGoal = ENPCDesire::Wander;
	bool bWantsRandomLocation = false;

	// Steht der NPC gerade an einem ECHTEN reservierten Spot (Bett/Stuhl/Amboss)?
	// Vom Character gesetzt. Unterscheidet "Erholung am Rest-Spot" (schnell) von
	// "Erholung beim Umherwandern" (langsam), wenn kein Spot frei war.
	bool bHasSpot = false;

	// Läuft die loopende Tätigkeits-Animation am Spot bereits? Vom Character gesetzt.
	// Erst dann zählen die Needs am Spot (während Intro/Hinlaufen noch nicht). Spots
	// ohne Loop-Montage setzen das sofort true (nichts zu warten).
	bool bLoopActive = false;
};

// ============================================================================
// ENTITY - Hält alle Komponenten für einen NPC
// ============================================================================

struct FNPCEntity
{
	EntityID ID = INVALID_ENTITY;
	ANPCCharacter* OwnerCharacter = nullptr; // Referenz zum Character, um Zugriff auf Actor-Componenten zu ermöglichen
	int32 ActiveSystemFlags = 0;
	// === KOMPONENTEN (Alle 13) ===

	// GRUPPE A: LLM-DATA
	FIdentityData Identity;
	FMemoryData Memory;
	FReflectionData Reflection;
	FEmotionData Emotion;
	FRelationshipData Relationship;
	FScheduleData Schedule;
	FDialogData Dialog;
	FConversationData Conversation;

	// GRUPPE B: EXECUTION
	FTransformData Transform;
	FNavMeshData NavMesh;
	FBTStateData BTState;

	// GRUPPE C: EXTENSION
	FNeedsData Needs;
	FWaypointData Waypoint;
	FInventoryData Inventory;   // Items + Gold (Wirtschaft/Quests)

	//Constructor
	FNPCEntity() = default;

	FNPCEntity(EntityID _id, ANPCCharacter* _owner) : ID(_id), OwnerCharacter(_owner)
	{
	}
};

// ============================================================================
// ECS - SYSTEMS (verarbeiten Komponenten)
// ============================================================================

class FNPCECSSystem
{
public:
	virtual ~FNPCECSSystem() = default;
	virtual void Update(FNPCEntity& _entities, float _deltaTime) = 0;
	
	virtual ESystemType GetSystemType() const = 0;
};

class FMemoryDecaySystem : public FNPCECSSystem
{
public:
	virtual void Update(FNPCEntity& _entity, float _deltaTime) override;
	virtual ESystemType GetSystemType() const override { return ESystemType::MemoryDecay; }
};

class FEmotionDecaySystem : public FNPCECSSystem
{
public:
	virtual void Update(FNPCEntity& _entity, float _deltaTime) override;
	virtual ESystemType GetSystemType() const override { return ESystemType::EmotionDecay; }
};

class FTransformSyncSystem : public FNPCECSSystem
{
public:
	virtual void Update(FNPCEntity& _entity, float _deltaTime) override;
	virtual ESystemType GetSystemType() const override { return ESystemType::TransformSync; }
};

class FNeedsUpdateSystem : public FNPCECSSystem
{
public:
	virtual void Update(FNPCEntity& _entity, float _deltaTime) override;
	virtual ESystemType GetSystemType() const override { return ESystemType::Needs; }
};

class FWanderSystem : public FNPCECSSystem
{
public:
	virtual void Update(FNPCEntity& _entity, float _deltaTime) override;
	virtual ESystemType GetSystemType() const override { return ESystemType::Wander; }
	
private:
	void DecideNextGoal(FNPCEntity& _entity) const;
};

class FScheduleSystem : public FNPCECSSystem
{
public:
	virtual void Update(FNPCEntity& _entity, float _deltaTime) override;
	virtual ESystemType GetSystemType() const override { return ESystemType::Schedule; }
	
	static int32 CurrentGameHour;
	// Wie viele Spielstunden pro Echtzeit-Sekunde vergehen (aus der WorldClock).
	// Erlaubt Needs in Spielzeit zu rechnen, statt in Echtzeit.
	static float GameHoursPerSecond;

private:
	ENPCDesire ActivityStringToDesire(const FString& _activity) const;
};

// ============================================================================
// MAIN ECS MANAGER
// ============================================================================

class FNPCECS
{
public:
	FNPCECS();
	~FNPCECS();

	// === ENTITY MANAGEMENT ===
	EntityID CreateEntity(ANPCCharacter* _owner);
	void DestroyEntity(EntityID _id);
	FNPCEntity* GetEntity(EntityID _id);
	const FNPCEntity* GetEntity(EntityID _id) const;

	// === UPDATE ===

	void Update(float _deltaTime);
	void DebugPrintEntity(EntityID _id) const;


	// === COMPONENT ACCESS ===

	template <typename ComponentType>
	ComponentType* GetComponent(EntityID _id);

	template <typename ComponentType>
	const ComponentType* GetComponent(EntityID _id) const;

private:
	TMap<EntityID, FNPCEntity> m_entities;
	// IDs werden global eindeutig in CreateEntity() vergeben (statischer
	// Zähler), nicht pro Instanz – sonst bekäme jeder NPC dieselbe ID 1.

	TArray<TUniquePtr<FNPCECSSystem>> Systems;

	void InitalizeSystems();
};

// ============================================================================
// TEMPLATE IMPLEMENTATIONS
// ============================================================================
template <typename ComponentType>
ComponentType* FNPCECS::GetComponent(EntityID ID)
{
	FNPCEntity* Entity = GetEntity(ID);
	if (!Entity) return nullptr;

	// Type dispatch based on component type
	if constexpr (std::is_same_v<ComponentType, FIdentityData>)
		return &Entity->Identity;
	else if constexpr (std::is_same_v<ComponentType, FMemoryData>)
		return &Entity->Memory;
	else if constexpr (std::is_same_v<ComponentType, FEmotionData>)
		return &Entity->Emotion;
	else if constexpr (std::is_same_v<ComponentType, FRelationshipData>)
		return &Entity->Relationship;
	else if constexpr (std::is_same_v<ComponentType, FScheduleData>)
		return &Entity->Schedule;
	else if constexpr (std::is_same_v<ComponentType, FTransformData>)
		return &Entity->Transform;
	else if constexpr (std::is_same_v<ComponentType, FNavMeshData>)
		return &Entity->NavMesh;
	else if constexpr (std::is_same_v<ComponentType, FBTStateData>)
		return &Entity->BTState;
	else if constexpr (std::is_same_v<ComponentType, FNeedsData>)
		return &Entity->Needs;
	else if constexpr (std::is_same_v<ComponentType, FWaypointData>)
		return &Entity->Waypoint;
	else if constexpr (std::is_same_v<ComponentType, FInventoryData>)
		return &Entity->Inventory;
	else if constexpr (std::is_same_v<ComponentType, FDialogData>)
		return &Entity->Dialog;
	else if constexpr (std::is_same_v<ComponentType, FReflectionData>)
		return &Entity->Reflection;
	else if constexpr (std::is_same_v<ComponentType, FConversationData>)
		return &Entity->Conversation;
	else
		return nullptr;
}

template <typename ComponentType>
const ComponentType* FNPCECS::GetComponent(EntityID ID) const
{
	const FNPCEntity* Entity = GetEntity(ID);
	if (!Entity) return nullptr;

	// Same as non-const version
	if constexpr (std::is_same_v<ComponentType, FIdentityData>)
		return &Entity->Identity;
	else if constexpr (std::is_same_v<ComponentType, FMemoryData>)
		return &Entity->Memory;
	else if constexpr (std::is_same_v<ComponentType, FEmotionData>)
		return &Entity->Emotion;
	else if constexpr (std::is_same_v<ComponentType, FRelationshipData>)
		return &Entity->Relationship;
	else if constexpr (std::is_same_v<ComponentType, FScheduleData>)
		return &Entity->Schedule;
	else if constexpr (std::is_same_v<ComponentType, FTransformData>)
		return &Entity->Transform;
	else if constexpr (std::is_same_v<ComponentType, FNavMeshData>)
		return &Entity->NavMesh;
	else if constexpr (std::is_same_v<ComponentType, FBTStateData>)
		return &Entity->BTState;
	else if constexpr (std::is_same_v<ComponentType, FNeedsData>)
		return &Entity->Needs;
	else if constexpr (std::is_same_v<ComponentType, FWaypointData>)
		return &Entity->Waypoint;
	else if constexpr (std::is_same_v<ComponentType, FInventoryData>)
		return &Entity->Inventory;
	else if constexpr (std::is_same_v<ComponentType, FDialogData>)
		return &Entity->Dialog;
	else if constexpr (std::is_same_v<ComponentType, FReflectionData>)
		return &Entity->Reflection;
	else if constexpr (std::is_same_v<ComponentType, FConversationData>)
		return &Entity->Conversation;
	else
		return nullptr;
}
