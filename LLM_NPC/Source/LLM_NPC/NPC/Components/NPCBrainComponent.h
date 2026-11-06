#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPCBrainComponent.generated.h"

class ANPCCharacter;
class FJsonValue;
struct FLLMResponse;
struct FNPCDialogResponse;
enum class EIntentType : uint8;
enum class EEmotionType : uint8;

/**
 * UNPCBrainComponent – die Actor-seitige Brücke zur LLM-Schicht (Kap. 3 der
 * Architektur). Sitzt auf jedem NPC-Actor, kennt seinen NPC (und damit dessen
 * EntityID) und bündelt die LLM-Methoden an EINER Stelle.
 *
 * Aktuell hier: die komplette Tagesplan-Schleife (Anfrage → Antwort → ins ECS
 * laden, inkl. Server-Warten und Retry).
 *
 * Weitere LLM-Aufgaben (Dialog-Anwendung, NPC↔NPC-Gespräche) können schrittweise
 * nach demselben Muster hierher wandern.
 */
UCLASS(ClassGroup=(NPC), meta=(BlueprintSpawnableComponent))
class LLM_NPC_API UNPCBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPCBrainComponent();

	// Lässt das LLM einen Tagesablauf planen und ins ECS des NPCs schreiben.
	//
	// Gedrosselter Einstieg: Routine-Anfragen (die BT-Schleife ruft das pro
	// Aktionszyklus) werden geschluckt, solange schon eine Anfrage läuft ODER der
	// Cooldown noch nicht abgelaufen ist. Echte Ereignisse (Tageswechsel, Quest,
	// gelernte Info, Bootstrap) setzen _bForce=true und umgehen die Drosselung.
	void RequestSchedule(bool _bForce = false);

	// Wendet ein Dialog-Ergebnis des LLM auf den NPC an: Emotion + Memory + Blackboard,
	// und plant bei Quest-Intent den Tag neu. Wird vom ChatWidget über den NPC gerufen.
	void ApplyDialogueResult(const FString& _reply, EIntentType _intent, EEmotionType _mood);

	// NPC↔NPC-Gespräch: entscheidet den Ausgang (LLM oder Offline-Formel), ändert die
	// Beziehung beidseitig und protokolliert es. Wird von StartConversationWith gerufen,
	// nachdem der Character Talk-State/Gesten aufgesetzt hat. _myGossip/_theirGossip sind
	// die beim Start erfassten Gerüchte (fürs Prompt + JSON).
	void RunConversation(ANPCCharacter* _other, const FString& _myGossip, const FString& _theirGossip);

	// Parst eine rohe LLM-Chat-Antwort (JSON) in Reply/Intent/Mood + optionale Quest.
	// Zentrale Stelle fürs LLM-JSON-Parsing – das ChatWidget nutzt nur das Ergebnis.
	FNPCDialogResponse ParseLLMResponse(const FString& _raw) const;

	// Reflexion: lässt das LLM aus den jüngsten Erinnerungen höhere
	// Einsichten ziehen und schreibt sie als hochgewichtige Erinnerungen zurück.
	// Wird ausgelöst, wenn genug Wichtigkeit angesammelt wurde (FReflectionData).
	void RequestReflection();

	// Zweite Verdichtungsstufe: destilliert die Gesprächs-Erinnerungen über _about zu
	// einer stabilen Meinung (FNPCRelation::Opinion). Wird ausgelöst, wenn seit der
	// letzten Meinung genug neue Erinnerungen dazukamen.
	void RequestOpinionUpdate(ANPCCharacter* _about);

	// Proaktive Begrüßung: erzeugt einen kurzen Eröffnungssatz und legt ihn im NPC ab, damit er beim Öffnen des
	// Chats sofort bereitsteht. Gedrosselt: kein Doppelrequest, Cooldown, nur wenn noch
	// keine Begrüßung wartet.
	void RequestGreeting();

protected:
	virtual void BeginPlay() override;

private:
	// Setzt die Anfrage tatsächlich ab (Server-Warten + Submit) – ungedrosselt.
	// Wird von RequestSchedule nach den Guards gerufen und von den internen Timern
	// (Server-Warten / Retry) direkt, da diese schon committete Folgeversuche sind.
	void DispatchSchedule();

	// Antwort der Schedule-Anfrage (per RequestID auf genau diese Anfrage gefiltert).
	UFUNCTION()
	void OnScheduleResponse(const FLLMResponse& _response, bool _bSuccess);

	// Parst die JSON-Antwort und schreibt den Plan in die ScheduleData des NPCs.
	bool LoadSchedule(const FString& _content);

	// --- NPC↔NPC-Gespräch (LLM-Pfad) ---
	void RequestConversation(ANPCCharacter* _other);
	FString BuildConversationPrompt(ANPCCharacter* _other) const;

	UFUNCTION()
	void OnConversationResponse(const FLLMResponse& _response, bool _bSuccess);

	void ApplyConversationOutcome(ANPCCharacter* _other, float _likingDelta,
		const FString& _outcome, const FString& _reason,
		const TArray<TSharedPtr<FJsonValue>>& _dialogue);

	void SaveConversationJson(ANPCCharacter* _other, const FString& _outcome,
		const FString& _reason, const TArray<TSharedPtr<FJsonValue>>& _dialogue);

	// --- Reflexion ---
	FString BuildReflectionPrompt() const;

	UFUNCTION()
	void OnReflectionResponse(const FLLMResponse& _response, bool _bSuccess);

	FString m_reflectionRequestID;

	// --- Meinung pro Person (zweite Verdichtungsstufe) ---
	FString BuildOpinionPrompt(ANPCCharacter* _about) const;

	UFUNCTION()
	void OnOpinionResponse(const FLLMResponse& _response, bool _bSuccess);

	FString m_opinionRequestID;
	TWeakObjectPtr<ANPCCharacter> m_opinionAbout;   // wen die laufende Meinungs-Anfrage betrifft

	// --- Proaktive Begrüßung ---
	FString BuildGreetingPrompt() const;

	UFUNCTION()
	void OnGreetingResponse(const FLLMResponse& _response, bool _bSuccess);

	FString m_greetingRequestID;
	float   m_lastGreetingRealTime = -1000.0f;   // Cooldown-Marke (Real-Zeit)

	// Der NPC, zu dem dieses Gehirn gehört (Zugriff auf ECS/NPCData/Prompt).
	UPROPERTY()
	TObjectPtr<ANPCCharacter> m_npc;

	// RequestID der laufenden Schedule-Anfrage (fürs Filtern der Antwort).
	// Nicht leer = eine Anfrage ist unterwegs → dient zugleich als In-Flight-Guard.
	FString m_scheduleRequestID;

	// Retry, falls die Anfrage scheitert (z.B. Server lädt noch / HTTP 503).
	FTimerHandle m_scheduleRetryTimer;
	int32        m_scheduleRetries = 0;

	// Weltzeit (s) der letzten abgesetzten Schedule-Anfrage – für den Routine-Cooldown.
	float m_lastScheduleRealTime = -10000.0f;

	// Mindestabstand (Echtzeit-Sekunden) zwischen zwei ROUTINE-Replans (BT-Schleife).
	// Tageswechsel/Quest/gelernte Info umgehen das über _bForce.
	UPROPERTY(EditAnywhere, Category = "NPC|Schedule")
	float ScheduleCooldownSeconds = 30.0f;

	// --- Laufende NPC↔NPC-Gesprächsanfrage ---
	FString m_convRequestID;
	TWeakObjectPtr<ANPCCharacter> m_convOutcomePartner;
	FString m_convGossipMine;
	FString m_convGossipTheirs;
};
