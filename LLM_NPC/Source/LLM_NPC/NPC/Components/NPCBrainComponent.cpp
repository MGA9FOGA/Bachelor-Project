#include "NPCBrainComponent.h"

#include "TimerManager.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "LLM_NPC/NPC/NPCCharacter.h"
#include "LLM_NPC/NPC/NPCDataAsset.h"
#include "LLM_NPC/LLM_NPCCharacter.h"          
#include "LLM_NPC/Economy/ItemDataAsset.h"
#include "LLM_NPC/LLM_Server/LLMRequestManager.h"
#include "LLM_NPC/LLM_Server/LLMSchemas.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

UNPCBrainComponent::UNPCBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


static void TryBuyConsumableFrom(ANPCCharacter* _buyer, ANPCCharacter* _seller)
{
	if (!_buyer || !_seller) return;

	const FNeedsData* needs = _buyer->GetNeeds();
	if (!needs) return;

	EConsumeType want;
	if (needs->IsHungry())         want = EConsumeType::Food;
	else if (needs->IsExhausted()) want = EConsumeType::Drink;
	else return;   

	UItemDataAsset* item = _seller->FindConsumable(want);
	if (!item) return;   

	// Preis: Grundpreis, um die Sympathie des VERKÄUFERS zum Käufer verschoben
	// (Freund billiger). −0.5..+0.5 → ±25 %.
	const float liking = _seller->GetLikingToward(_buyer->GetEntityID());
	const int32 price  = FMath::Max(1, FMath::RoundToInt(item->BasePrice * (1.0f - (liking - 0.5f) * 0.5f)));

	if (_buyer->GetGold() < price) return;   // zu arm
	if (!_buyer->SpendGold(price)) return;

	_buyer->AddItem(item->ItemId, 1);
	_seller->RemoveItem(item->ItemId, 1);
	_seller->AddGold(price);

	UE_LOG(LogTemp, Log, TEXT("[Handel] %s kauft '%s' von %s fuer %d Gold"),
		   *_buyer->GetNPCName(), *item->DisplayName, *_seller->GetNPCName(), price);
}

void UNPCBrainComponent::BeginPlay()
{
	Super::BeginPlay();
	m_npc = Cast<ANPCCharacter>(GetOwner());
}

// ============================================================================
//  Dialog-Ergebnis des LLM anwenden (Emotion + Memory + Blackboard, ggf. Replan)
// ============================================================================
void UNPCBrainComponent::ApplyDialogueResult(const FString& _reply, EIntentType _intent, EEmotionType _mood)
{
	if (!m_npc) return;

	UWorld* world  = m_npc->GetWorld();
	const float now = world ? world->GetTimeSeconds() : 0.0f;

	if (FEmotionData* emotion = m_npc->GetEmotion())
		emotion->TriggerEmotion(_mood, 0.7f, now);

	// Stimmung → Spieler-Sympathie: ein freundliches Gespräch wärmt, ein feindliches
	// kühlt. Das beeinflusst u.a. die Marktpreise, die dieser NPC dem Spieler macht.
	const float g = m_npc->FriendshipGainPerChat;
	float likingDelta = 0.1f * g;   
	switch (_mood)
	{
		case EEmotionType::Happy:      likingDelta =  g;         break;
		case EEmotionType::Angry:      likingDelta = -g;         break;
		case EEmotionType::Suspicious: likingDelta = -0.5f * g;  break;
		case EEmotionType::Concerned:  likingDelta = -0.25f * g; break;
		default: break;   
	}
	m_npc->AdjustPlayerLiking(likingDelta);

	// Erinnerung ablegen (+ Reflexions-Wichtigkeit ansammeln)
	if (FMemoryData* memory = m_npc->GetMemory())
	{
		const FString memEntry = FString::Printf(TEXT("I replied: %s"), *_reply);
		memory->AddMemory(memEntry, 0.5f, now);
		if (FReflectionData* refl = m_npc->GetReflection())
			if (refl->Accumulate(0.5f))
				RequestReflection();
	}

	// Blackboard für den BT (LastIntent / im Dialog)
	if (AAIController* AIC = Cast<AAIController>(m_npc->GetController()))
	{
		if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
		{
			BB->SetValueAsString(TEXT("LastIntent"), IntentToString(_intent));
			BB->SetValueAsBool(TEXT("IsInDialogue"), true);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] Intent: %s | Mood: %s"),
		   *m_npc->GetNPCName(), *IntentToString(_intent), *EmotionToMoodString(_mood));
	UE_LOG(LogTemp, Warning, TEXT("[%s] Reply: %s"), *m_npc->GetNPCName(), *_reply);

	// Neuplanung nur bei neuer, planrelevanter Info.
	float infoImportance = 0.3f;                                  // Greeting/Neutral: Geplauder
	switch (_intent)
	{
		case EIntentType::Quest:   infoImportance = 0.85f; break; // Aufgabe/Info → planrelevant
		case EIntentType::Hostile: infoImportance = 0.65f; break; // Drohung → NPC passt Tag an
		case EIntentType::Trade:   infoImportance = 0.40f; break; // Handel: kein Tagesumbau
		default: break;
	}

	if (_intent == EIntentType::Quest)
	{
		// Quest immer + sofort umplanen (echtes Ereignis → Cooldown umgehen).
		m_npc->ReplanSchedule(/*bForce*/ true);
	}
	else if (infoImportance >= m_npc->ScheduleReplanMinImportance)
	{
		// Sonstige wichtige Info → umplanen, aber gedrosselt (Cooldown/In-Flight-Guard).
		m_npc->ReplanSchedule(/*bForce*/ false);
	}
}

// ============================================================================
//  Tagesplan anfragen – GEDROSSELTER Einstieg
// ----------------------------------------------------------------------------
//  Die BT-Schleife ruft das pro Aktionszyklus (Knoten "Plan Day"); das flutete
//  die LLM-Server. Zwei Guards halten Routine-Anfragen zurück:
//    1) In-Flight: läuft schon eine Anfrage (m_scheduleRequestID gesetzt) → raus.
//    2) Cooldown: seit der letzten Anfrage < ScheduleCooldownSeconds → raus.
// ============================================================================
void UNPCBrainComponent::RequestSchedule(bool _bForce)
{
	if (!m_npc) return;

	UWorld* world = m_npc->GetWorld();
	const float now = world ? world->GetTimeSeconds() : 0.0f;

	if (!_bForce)
	{
		if (!m_scheduleRequestID.IsEmpty())
			return;

		if ((now - m_lastScheduleRealTime) < ScheduleCooldownSeconds)
			return;
	}

	m_lastScheduleRealTime = now;
	DispatchSchedule();
}

// ============================================================================
//  Tagesplan wirklich absetzen (Server-Warten + Submit) – UNGEDROSSELT.
//  Nur aus RequestSchedule (nach den Guards) oder den internen Timern rufen.
// ============================================================================
void UNPCBrainComponent::DispatchSchedule()
{
	if (!m_npc) return;

	ALLMRequestManager* mgr = ALLMRequestManager::Get();
	if (!mgr) return;

	UWorld* world = m_npc->GetWorld();

	if (!ALLMRequestManager::AreServersReady(world))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[%s] Warte auf Server-Bereitschaft, bevor geplant wird …"),
			   *m_npc->GetNPCName());
		if (world)
			world->GetTimerManager().SetTimer(m_scheduleRetryTimer, this,
				&UNPCBrainComponent::DispatchSchedule, 2.0f, false);
		return;
	}

	const FString prompt = m_npc->GenerateSchedulePrompt();
	if (prompt.IsEmpty()) return;   

	mgr->OnResponseReceived.AddDynamic(this, &UNPCBrainComponent::OnScheduleResponse);

	FLLMRequest req;
	req.RequestID    = FGuid::NewGuid().ToString();
	req.NPCName      = m_npc->GetNPCName();
	req.SystemPrompt = TEXT("You are a daily-schedule planner. Respond ONLY with the requested JSON, nothing else.");
	req.UserMessage  = prompt;
	req.MaxTokens    = 512;   
	req.JsonSchema   = LLMSchemas::Schedule();
	req.Priority     = static_cast<int32>(ELLMPriority::Low);
	req.Timestamp    = world ? world->GetTimeSeconds() : 0.0f;

	m_scheduleRequestID = req.RequestID;
	mgr->SubmitRequest(req);

	UE_LOG(LogTemp, Warning, TEXT("[%s] LLM-Schedule angefragt (RequestID=%s)"),
		   *m_npc->GetNPCName(), *m_scheduleRequestID);
}

// ============================================================================
//  Antwort verarbeiten (mit Retry, falls der Server noch lädt)
// ============================================================================
void UNPCBrainComponent::OnScheduleResponse(const FLLMResponse& _response, bool _bSuccess)
{
	if (!m_npc) return;
	if (_response.RequestID != m_scheduleRequestID) return;   // nicht meine Anfrage

	// Antwort nur einmal nötig → wieder abmelden
	if (ALLMRequestManager* mgr = ALLMRequestManager::Get())
		mgr->OnResponseReceived.RemoveDynamic(this, &UNPCBrainComponent::OnScheduleResponse);
	m_scheduleRequestID.Empty();

	if (!_bSuccess || !LoadSchedule(_response.Content))
	{

		constexpr int32 maxRetries = 6;
		constexpr float retryDelay = 5.0f;   
		if (m_scheduleRetries < maxRetries)
		{
			m_scheduleRetries++;


			if (UWorld* w = m_npc->GetWorld())
				w->GetTimerManager().SetTimer(m_scheduleRetryTimer, this,
					&UNPCBrainComponent::DispatchSchedule, retryDelay, false);
		}
		else
		{

			m_scheduleRetries = 0;   
		}
		return;
	}

	m_scheduleRetries = 0;   

	if (FScheduleData* sched = m_npc->GetSchedule())
		UE_LOG(LogTemp, Warning, TEXT("[%s] LLM-Schedule uebernommen (%d Eintraege)"),
			   *m_npc->GetNPCName(), sched->DailySchedule.Num());
}

// ============================================================================
//  JSON-Plan parsen und in die ScheduleData des NPCs schreiben
// ============================================================================
bool UNPCBrainComponent::LoadSchedule(const FString& _content)
{
	if (!m_npc) return false;

	FScheduleData* sched = m_npc->GetSchedule();
	if (!sched) return false;

	FString json = _content.TrimStartAndEnd();
	if (!json.StartsWith(TEXT("{")))
		json = TEXT("{") + json;

	TSharedPtr<FJsonObject> root;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(json);
	if (!FJsonSerializer::Deserialize(reader, root) || !root.IsValid())
		return false;

	const TArray<TSharedPtr<FJsonValue>>* arr = nullptr;
	if (!root->TryGetArrayField(TEXT("schedule"), arr) || !arr)
		return false;

	TArray<FDayPlaynEntry> entries;
	for (const TSharedPtr<FJsonValue>& v : *arr)
	{
		const TSharedPtr<FJsonObject> obj = v->AsObject();
		if (!obj.IsValid()) continue;

		FDayPlaynEntry e;
		int32 hour = 0;
		obj->TryGetNumberField(TEXT("hour"), hour);
		e.Hour = hour;
		obj->TryGetStringField(TEXT("action"), e.Activity);

		e.Activity = e.Activity.ToLower();

		if (e.Activity == TEXT("wake")  || e.Activity == TEXT("wakeup") ||
			e.Activity == TEXT("wake up") || e.Activity == TEXT("awake"))
			continue;

		// Halluzinierte/ungültige Aktionen abfangen → auf "wander" normalisieren.
		static const TSet<FString> validActions = {
			TEXT("work"), TEXT("eat"), TEXT("drink"), TEXT("rest"), TEXT("wander"),
			TEXT("idle"), TEXT("sleep"), TEXT("shop") };
		if (!validActions.Contains(e.Activity))
			e.Activity = TEXT("wander");

		// KEIN Schlafen am Tag: ein sleep-Eintrag außerhalb der Nacht (~19–5 Uhr)
		// wird zu "rest"
		if (e.Activity == TEXT("sleep") && e.Hour > 5 && e.Hour < 19)
		{
			e.Activity = TEXT("rest");
			UE_LOG(LogTemp, Log, TEXT("[%s] Tag-Schlaf um %02d:00 → in 'rest' umgewandelt"),
				   *m_npc->GetNPCName(), e.Hour);
		}

		entries.Add(e);
	}
	if (entries.Num() == 0) return false;

	if (m_npc->NPCData)
	{
		const bool bHasSleep = entries.ContainsByPredicate(
			[](const FDayPlaynEntry& e){ return e.Activity == TEXT("sleep"); });
		if (!bHasSleep)
		{
			FDayPlaynEntry s;
			s.Hour     = SleepHourForType(m_npc->NPCData->SleepType);
			s.Activity = TEXT("sleep");
			entries.Add(s);
			UE_LOG(LogTemp, Log, TEXT("[%s] Kein Schlaf im LLM-Plan – Schlaf um %02d:00 ergaenzt"),
				   *m_npc->GetNPCName(), s.Hour);
		}
	}

	entries.Sort([](const FDayPlaynEntry& A, const FDayPlaynEntry& B){ return A.Hour < B.Hour; });

	sched->DailySchedule        = entries;
	sched->CurrentActivityIndex = -1;
	sched->bScheduleReady       = true;
	return true;
}

// ============================================================================
//  NPC↔NPC-Gespräch
// ============================================================================
static float PairCompatibility(EntityID _a, EntityID _b)
{
	const uint32 lo = FMath::Min(_a, _b);
	const uint32 hi = FMath::Max(_a, _b);
	const uint32 h  = HashCombine(GetTypeHash(lo), GetTypeHash(hi));
	const float  r  = static_cast<float>(h % 1000) / 1000.0f;   // 0..1, deterministisch
	return r * 1.5f - 0.5f;                                     // -0.5 .. +1.0
}

void UNPCBrainComponent::RunConversation(ANPCCharacter* _other, const FString& _myGossip, const FString& _theirGossip)
{
	if (!m_npc || !_other) return;

	m_convGossipMine   = _myGossip;
	m_convGossipTheirs = _theirGossip;


	if (m_npc->bLLMDrivenConversations && ALLMRequestManager::AreServersReady(m_npc->GetWorld()))
	{
		RequestConversation(_other);   
	}
	else
	{
		const float compat = PairCompatibility(m_npc->GetEntityID(), _other->GetEntityID());  // -0.5..+1.0
		const FString outcome = compat > 0.15f ? TEXT("positive")
							  : (compat < -0.05f ? TEXT("negative") : TEXT("neutral"));
		ApplyConversationOutcome(_other, m_npc->FriendshipGainPerChat * compat, outcome,
			TEXT("(offline: Kompatibilitaets-Formel)"), {});
	}
}

void UNPCBrainComponent::RequestConversation(ANPCCharacter* _other)
{
	ALLMRequestManager* mgr = ALLMRequestManager::Get();
	if (!mgr || !_other || !m_npc) return;

	m_convOutcomePartner = _other;
	mgr->OnResponseReceived.AddUniqueDynamic(this, &UNPCBrainComponent::OnConversationResponse);

	FLLMRequest req;
	req.RequestID    = FGuid::NewGuid().ToString();
	req.NPCName      = m_npc->GetNPCName();
	req.SystemPrompt = TEXT("You simulate brief NPC-to-NPC small talk in a medieval village. All dialogue and text must be in English only. Respond ONLY with the requested JSON.");
	req.UserMessage  = BuildConversationPrompt(_other);
	req.MaxTokens    = 256;
	req.JsonSchema   = LLMSchemas::Conversation();
	req.Priority     = static_cast<int32>(ELLMPriority::Low);
	req.Timestamp    = m_npc->GetWorld() ? m_npc->GetWorld()->GetTimeSeconds() : 0.0f;

	m_convRequestID = req.RequestID;
	mgr->SubmitRequest(req);

	UE_LOG(LogTemp, Log, TEXT("[%s] NPC-Gespraech ans LLM (mit %s)"),
		   *m_npc->GetNPCName(), *_other->GetNPCName());
}

FString UNPCBrainComponent::BuildConversationPrompt(ANPCCharacter* _other) const
{
	const FString me        = m_npc->GetNPCName();
	const FString them      = _other->GetNPCName();
	const FString myOcc     = m_npc->NPCData ? m_npc->NPCData->GetOccupationString() : TEXT("villager");
	const FString theirOcc  = _other->NPCData ? _other->NPCData->GetOccupationString() : TEXT("villager");
	const FString myPers    = m_npc->NPCData ? m_npc->NPCData->Personality : TEXT("");
	const FString theirPers = _other->NPCData ? _other->NPCData->Personality : TEXT("");

	float myLiking = 0.5f;   // aktuelle Sympathie (falls schon eine Beziehung besteht)
	if (const FRelationshipData* rel = m_npc->GetRelationships())
		if (const FNPCRelation* r = rel->FindRelation(_other->GetEntityID()))
			myLiking = r->Liking;

	FString prompt;
	prompt += TEXT("Two villagers meet and chat briefly.\n");
	prompt += FString::Printf(TEXT("%s is a %s. Personality: %s\n"), *me, *myOcc, *myPers);
	prompt += FString::Printf(TEXT("%s is a %s. Personality: %s\n"), *them, *theirOcc, *theirPers);
	prompt += FString::Printf(TEXT("%s currently likes %s at %.0f%%.\n"), *me, *them, myLiking * 100.0f);
	if (!m_convGossipMine.IsEmpty())
		prompt += FString::Printf(TEXT("%s might mention: \"%s\".\n"), *me, *m_convGossipMine);


	if (const FRelationshipData* relMem = m_npc->GetRelationships())
		if (const FNPCRelation* r = relMem->FindRelation(_other->GetEntityID()))
		{
			if (!r->Opinion.IsEmpty())
				prompt += FString::Printf(TEXT("%s's opinion of %s: %s\n"), *me, *them, *r->Opinion);
			if (r->Memories.Num() > 0)
			{
				prompt += FString::Printf(TEXT("What %s remembers about %s from past talks:\n"), *me, *them);
				for (const FString& mText : r->Memories)
					prompt += FString::Printf(TEXT("- %s\n"), *mText);
			}
		}

	prompt += TEXT("\nBased on their personalities and what they remember of each other, write a short 2-4 line chat and decide whether they get along.\n");
	prompt += TEXT("Respond with ONLY this JSON on a single line, nothing else:\n");
	prompt += FString::Printf(
		TEXT("{\"dialogue\":[{\"speaker\":\"%s\",\"text\":\"...\"},{\"speaker\":\"%s\",\"text\":\"...\"}],\"outcome\":\"positive|neutral|negative\",\"reason\":\"short reason\",\"memory\":\"one short sentence they will both remember about this chat\"}"),
		*me, *them);
	return prompt;
}

void UNPCBrainComponent::OnConversationResponse(const FLLMResponse& _response, bool _bSuccess)
{
	if (_response.RequestID != m_convRequestID) return;   

	if (ALLMRequestManager* mgr = ALLMRequestManager::Get())
		mgr->OnResponseReceived.RemoveDynamic(this, &UNPCBrainComponent::OnConversationResponse);
	m_convRequestID.Empty();

	ANPCCharacter* other = m_convOutcomePartner.Get();
	if (!other || !m_npc) return;   

	FString outcome = TEXT("neutral");
	FString reason;
	FString memory;
	TArray<TSharedPtr<FJsonValue>> dialogue;

	if (_bSuccess)
	{
		FString json = _response.Content.TrimStartAndEnd();
		if (!json.StartsWith(TEXT("{"))) json = TEXT("{") + json;
		TSharedPtr<FJsonObject> root;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(json);
		if (FJsonSerializer::Deserialize(reader, root) && root.IsValid())
		{
			root->TryGetStringField(TEXT("outcome"), outcome);
			root->TryGetStringField(TEXT("reason"),  reason);
			root->TryGetStringField(TEXT("memory"),  memory);
			const TArray<TSharedPtr<FJsonValue>>* arr = nullptr;
			if (root->TryGetArrayField(TEXT("dialogue"), arr) && arr)
				dialogue = *arr;
		}
	}

	// Verdichtete Beziehungs-Erinnerung bei BEIDEN ablegen (je Partner) 
	{
		if (FRelationshipData* relMine = m_npc->GetRelationships())
		{
			relMine->AddMemory(other->GetEntityID(), other->GetNPCName(), memory);
			// Zweite Stufe: genug neue Erinnerungen mit dieser Person → Meinung bilden.
			if (const FNPCRelation* r = relMine->FindRelation(other->GetEntityID()))
				if (r->MemoriesSinceOpinion >= FNPCRelation::MAX_REL_MEMORIES)
					RequestOpinionUpdate(other);
		}
		if (FRelationshipData* relOther = other->GetRelationships())
			relOther->AddMemory(m_npc->GetEntityID(), m_npc->GetNPCName(), memory);
	}

	// Diagnose: hat das LLM Dialogzeilen geliefert? 0 = Offline-Fallback oder leere Antwort
	UE_LOG(LogTemp, Log, TEXT("[Sprechblase] %s <-> %s: Gespräch Erfolg=%s, Dialogzeilen=%d"),
		   *m_npc->GetNPCName(), *other->GetNPCName(), _bSuccess ? TEXT("ja") : TEXT("nein"), dialogue.Num());

	// Die vom LLM erzeugten Dialogzeilen in den ECS-Gesprächsverlauf beider NPCs
	// aufnehmen ("Speaker: Text")
	if (dialogue.Num() > 0)
	{
		const float now = m_npc->GetWorld() ? m_npc->GetWorld()->GetTimeSeconds() : 0.0f;
		FConversationData* convMine   = m_npc->GetConversation();
		FConversationData* convOther  = other->GetConversation();

		TArray<FString> speakers, texts;
		for (const TSharedPtr<FJsonValue>& d : dialogue)
		{
			const TSharedPtr<FJsonObject> o = d.IsValid() ? d->AsObject() : nullptr;
			if (!o.IsValid()) continue;
			FString speaker, text;
			o->TryGetStringField(TEXT("speaker"), speaker);
			o->TryGetStringField(TEXT("text"),    text);
			if (text.IsEmpty()) continue;
			speakers.Add(speaker);
			texts.Add(text);
			const FString line = FString::Printf(TEXT("%s: %s"), *speaker, *text);
			if (convMine)  convMine->AddTurn(line, now);
			if (convOther) convOther->AddTurn(line, now);
		}

		// Der Initiator (m_npc) treibt den Gesten-Takt und zeigt die Zeilen als
		// Sprechblasen über dem jeweiligen Sprecher.
		m_npc->SetConversationSubtitles(speakers, texts);
	}

	// Ausgang → Sympathie-Delta 
	outcome = outcome.ToLower();
	float delta = m_npc->FriendshipGainPerChat * 0.2f;              // neutral: leicht positiv
	if (outcome.Contains(TEXT("pos")))      delta =  m_npc->FriendshipGainPerChat;
	else if (outcome.Contains(TEXT("neg"))) delta = -m_npc->FriendshipGainPerChat;

	ApplyConversationOutcome(other, delta, outcome, reason, dialogue);
}

void UNPCBrainComponent::ApplyConversationOutcome(ANPCCharacter* _other, float _likingDelta,
	const FString& _outcome, const FString& _reason, const TArray<TSharedPtr<FJsonValue>>& _dialogue)
{
	if (!_other || !m_npc) return;
	const float now = m_npc->GetWorld() ? m_npc->GetWorld()->GetTimeSeconds() : 0.0f;

	// Beziehung beidseitig 
	if (FRelationshipData* myRel = m_npc->GetRelationships())
		myRel->AdjustRelation(_other->GetEntityID(), _other->GetNPCName(),
			_likingDelta, _likingDelta * 0.6f, now);
	if (FRelationshipData* otherRel = _other->GetRelationships())
		otherRel->AdjustRelation(m_npc->GetEntityID(), m_npc->GetNPCName(),
			_likingDelta, _likingDelta * 0.6f, now);


	TryBuyConsumableFrom(m_npc, _other);
	TryBuyConsumableFrom(_other, m_npc);

	if (m_npc->bLogConversationsToJson)
		SaveConversationJson(_other, _outcome, _reason, _dialogue);

}

void UNPCBrainComponent::SaveConversationJson(ANPCCharacter* _other, const FString& _outcome,
	const FString& _reason, const TArray<TSharedPtr<FJsonValue>>& _dialogue)
{
	if (!_other || !m_npc) return;

	const FDateTime nowTime = FDateTime::Now();
	const FString   stamp   = nowTime.ToString(TEXT("%Y%m%d_%H%M%S"));

	const TSharedRef<FJsonObject> root = MakeShared<FJsonObject>();
	root->SetStringField(TEXT("time"), stamp);
	root->SetNumberField(TEXT("gameHour"), FScheduleSystem::CurrentGameHour);
	root->SetStringField(TEXT("npcA"), m_npc->GetNPCName());
	root->SetStringField(TEXT("npcB"), _other->GetNPCName());
	root->SetStringField(TEXT("outcome"), _outcome);
	root->SetStringField(TEXT("reason"),  _reason);

	root->SetArrayField(TEXT("dialogue"), _dialogue);

	TArray<TSharedPtr<FJsonValue>> gossip;
	auto addGossip = [&gossip](const FString& _from, const FString& _to, const FString& _info)
	{
		if (_info.IsEmpty()) return;
		const TSharedRef<FJsonObject> g = MakeShared<FJsonObject>();
		g->SetStringField(TEXT("from"), _from);
		g->SetStringField(TEXT("to"),   _to);
		g->SetStringField(TEXT("info"), _info);
		gossip.Add(MakeShared<FJsonValueObject>(g));
	};
	addGossip(m_npc->GetNPCName(), _other->GetNPCName(), m_convGossipMine);
	addGossip(_other->GetNPCName(), m_npc->GetNPCName(), m_convGossipTheirs);
	root->SetArrayField(TEXT("gossip"), gossip);

	// Beziehungswerte NACH dem Gespräch (beide Richtungen).
	auto relObj = [](ANPCCharacter* _from, ANPCCharacter* _to)
	{
		const TSharedRef<FJsonObject> o = MakeShared<FJsonObject>();
		if (FRelationshipData* rel = _from->GetRelationships())
		{
			const FNPCRelation& r = rel->GetOrAddRelation(_to->GetEntityID());
			o->SetNumberField(TEXT("liking"), r.Liking);
			o->SetNumberField(TEXT("trust"),  r.Trust);
		}
		return o;
	};
	const TSharedRef<FJsonObject> rels = MakeShared<FJsonObject>();
	rels->SetObjectField(TEXT("aToB"), relObj(m_npc, _other));
	rels->SetObjectField(TEXT("bToA"), relObj(_other, m_npc));
	root->SetObjectField(TEXT("relationship"), rels);

	// Serialisieren + speichern (Saved/NPCConversations/).
	FString output;
	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&output);
	FJsonSerializer::Serialize(root, writer);

	const FString safeA = m_npc->GetNPCName().Replace(TEXT(" "), TEXT("_"));
	const FString safeB = _other->GetNPCName().Replace(TEXT(" "), TEXT("_"));
	const FString dir   = FPaths::ProjectSavedDir() / TEXT("NPCConversations");
	IFileManager::Get().MakeDirectory(*dir, /*Tree*/ true);
	const FString path  = dir / FString::Printf(TEXT("Conv_%s_%s_%s.json"), *safeA, *safeB, *stamp);

	if (FFileHelper::SaveStringToFile(output, *path))
		UE_LOG(LogTemp, Log, TEXT("[Gespräch] %s <-> %s (%s) -> %s"),
			   *m_npc->GetNPCName(), *_other->GetNPCName(), *_outcome, *path);
}

// ============================================================================
//  LLM-Chat-Antwort parsen (zentral) – Reply/Intent/Mood + optionale Quest.
// ============================================================================
FNPCDialogResponse UNPCBrainComponent::ParseLLMResponse(const FString& _raw) const
{
	FNPCDialogResponse result;

	FString json = _raw.TrimStartAndEnd();
	if (!json.StartsWith(TEXT("{")))
		json = TEXT("{") + json;

	int32 lastBrace = INDEX_NONE;
	if (json.FindLastChar('}', lastBrace))
		json = json.Left(lastBrace + 1);

	TSharedPtr<FJsonObject> obj;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(json);
	if (!FJsonSerializer::Deserialize(reader, obj) || !obj.IsValid())
	{
		result.Reply = _raw;   // Fallback: Rohtext, bParseSuccess bleibt false
		return result;
	}

	obj->TryGetStringField(TEXT("reply"), result.Reply);

	FString intentStr, moodStr;
	obj->TryGetStringField(TEXT("intent"), intentStr);
	obj->TryGetStringField(TEXT("mood"),   moodStr);
	result.Intent = StringToIntent(intentStr);
	result.Mood   = MoodStringToEmotion(moodStr);
	result.bParseSuccess = true;

	// Optionale Quest (vom LLM im selben JSON) → in das Ergebnis packen.
	const TSharedPtr<FJsonObject>* questObj = nullptr;
	if (obj->TryGetObjectField(TEXT("quest"), questObj) && questObj && questObj->IsValid())
	{
		FQuestData& q = result.Quest;

		FString kindStr;
		(*questObj)->TryGetStringField(TEXT("kind"), kindStr);
		const bool bDelivery = kindStr.Equals(TEXT("delivery"), ESearchCase::IgnoreCase);
		q.Type = bDelivery ? EQuestType::Delivery : EQuestType::Fetch;

		(*questObj)->TryGetStringField(TEXT("title"),       q.Title);
		(*questObj)->TryGetStringField(TEXT("description"), q.Description);
		int32 reward = 0; (*questObj)->TryGetNumberField(TEXT("reward"), reward);
		q.RewardGold  = FMath::Max(0, reward);
		q.GiverName   = m_npc ? m_npc->GetNPCName() : FString();
		q.QuestId     = FGuid::NewGuid().ToString();

		if (bDelivery)
		{
			// Boten-Quest: Ziel-NPC nötig, aber nicht der Geber selbst.
			(*questObj)->TryGetStringField(TEXT("target"), q.TargetNpc);
			q.TargetNpc = q.TargetNpc.TrimStartAndEnd();
			result.bHasQuest = !q.TargetNpc.IsEmpty()
				&& !q.TargetNpc.Equals(q.GiverName, ESearchCase::IgnoreCase);
		}
		else
		{
			FString itemStr;
			(*questObj)->TryGetStringField(TEXT("item"), itemStr);
			q.TargetItem = FName(*itemStr);
			int32 count = 1; (*questObj)->TryGetNumberField(TEXT("count"), count);
			q.TargetCount = FMath::Max(1, count);
			result.bHasQuest = !q.TargetItem.IsNone();
		}
	}

	return result;
}

// ============================================================================
//  Reflexion (Park et al. 2023): höhere Einsichten aus den jüngsten Erinnerungen
// ============================================================================
FString UNPCBrainComponent::BuildReflectionPrompt() const
{
	FMemoryData* mem = m_npc ? m_npc->GetMemory() : nullptr;
	if (!mem) return FString();

	const TArray<FMemoryEntry> top = mem->GetTopMemories(8);
	if (top.Num() == 0) return FString();

	const FString me   = m_npc->GetNPCName();
	const FString role = m_npc->NPCData ? m_npc->NPCData->GetOccupationString() : TEXT("villager");

	FString prompt;
	prompt += FString::Printf(TEXT("You are %s, a %s. Reflect on your recent experiences.\n"), *me, *role);
	prompt += TEXT("Recent memories:\n");
	for (const FMemoryEntry& e : top)
		prompt += FString::Printf(TEXT("- %s\n"), *e.Content);
	prompt += TEXT("\nBased on these, state 2-3 short, high-level insights about yourself, other people, or the village.\n");
	prompt += TEXT("Respond with ONLY this JSON on a single line, nothing else:\n");
	prompt += TEXT("{\"reflections\":[\"insight one\",\"insight two\"]}");
	return prompt;
}

void UNPCBrainComponent::RequestReflection()
{
	if (!m_npc) return;

	FReflectionData* refl = m_npc->GetReflection();
	if (!refl || refl->bRequestInFlight) return;   // schon eine Reflexion unterwegs

	ALLMRequestManager* mgr = ALLMRequestManager::Get();
	if (!mgr || !ALLMRequestManager::AreServersReady(m_npc->GetWorld()))
		return;   // best-effort: bei nicht bereiten Servern später erneut ausgelöst

	const FString prompt = BuildReflectionPrompt();
	if (prompt.IsEmpty()) return;

	mgr->OnResponseReceived.AddUniqueDynamic(this, &UNPCBrainComponent::OnReflectionResponse);

	FLLMRequest req;
	req.RequestID    = FGuid::NewGuid().ToString();
	req.NPCName      = m_npc->GetNPCName();
	req.SystemPrompt = TEXT("You are a reflective mind distilling higher-level insights from memories. Write all text in English only. Respond ONLY with the requested JSON.");
	req.UserMessage  = prompt;
	req.MaxTokens    = 200;
	req.JsonSchema   = LLMSchemas::Reflection();
	req.Priority     = static_cast<int32>(ELLMPriority::Low);
	req.Timestamp    = m_npc->GetWorld() ? m_npc->GetWorld()->GetTimeSeconds() : 0.0f;

	m_reflectionRequestID  = req.RequestID;
	refl->bRequestInFlight  = true;
	mgr->SubmitRequest(req);

}

void UNPCBrainComponent::OnReflectionResponse(const FLLMResponse& _response, bool _bSuccess)
{
	if (_response.RequestID != m_reflectionRequestID) return;   // nicht meine Anfrage

	if (ALLMRequestManager* mgr = ALLMRequestManager::Get())
		mgr->OnResponseReceived.RemoveDynamic(this, &UNPCBrainComponent::OnReflectionResponse);
	m_reflectionRequestID.Empty();

	if (!m_npc) return;
	FReflectionData* refl = m_npc->GetReflection();
	if (refl) refl->bRequestInFlight = false;
	if (!refl || !_bSuccess) return;

	// JSON robust einlesen (Worker-Primer '{' ggf. voranstellen, Rest nach '}' kappen).
	FString json = _response.Content.TrimStartAndEnd();
	if (!json.StartsWith(TEXT("{"))) json = TEXT("{") + json;
	int32 lastBrace = INDEX_NONE;
	if (json.FindLastChar('}', lastBrace)) json = json.Left(lastBrace + 1);

	TSharedPtr<FJsonObject> root;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(json);
	if (!FJsonSerializer::Deserialize(reader, root) || !root.IsValid()) return;

	const TArray<TSharedPtr<FJsonValue>>* arr = nullptr;
	if (!root->TryGetArrayField(TEXT("reflections"), arr) || !arr) return;

	const float now = m_npc->GetWorld() ? m_npc->GetWorld()->GetTimeSeconds() : 0.0f;
	FMemoryData* mem = m_npc->GetMemory();

	int32 added = 0;
	for (const TSharedPtr<FJsonValue>& v : *arr)
	{
		const FString insight = v->AsString().TrimStartAndEnd();
		if (insight.IsEmpty()) continue;

		refl->Reflections.Add(insight);

		if (mem) mem->AddMemory(FString::Printf(TEXT("Insight: %s"), *insight), 0.85f, now);
		++added;
	}

	if (added > 0)
	{
		refl->ResetAccumulation(now);

	}
}

// ============================================================================
//  Meinung pro Person – zweite Verdichtungsstufe der Gesprächs-Erinnerungen
// ============================================================================
FString UNPCBrainComponent::BuildOpinionPrompt(ANPCCharacter* _about) const
{
	if (!m_npc || !_about) return FString();

	const FRelationshipData* rel = m_npc->GetRelationships();
	const FNPCRelation* r = rel ? rel->FindRelation(_about->GetEntityID()) : nullptr;
	if (!r) return FString();

	const FString me   = m_npc->GetNPCName();
	const FString them = _about->GetNPCName();

	FString prompt;
	prompt += FString::Printf(TEXT("You are %s. Form your overall opinion of %s.\n"), *me, *them);
	if (!r->Opinion.IsEmpty())
		prompt += FString::Printf(TEXT("Your current opinion: %s\n"), *r->Opinion);
	prompt += FString::Printf(TEXT("You currently like %s at %.0f%% and trust them at %.0f%%.\n"),
		*them, r->Liking * 100.0f, r->Trust * 100.0f);
	if (r->Memories.Num() > 0)
	{
		prompt += TEXT("Recent moments with them:\n");
		for (const FString& mText : r->Memories)
			prompt += FString::Printf(TEXT("- %s\n"), *mText);
	}
	prompt += TEXT("\nUpdate your opinion of them into ONE short sentence (their character and where you stand with them).\n");
	prompt += TEXT("Respond with ONLY this JSON on a single line, nothing else:\n");
	prompt += TEXT("{\"opinion\":\"...\"}");
	return prompt;
}

void UNPCBrainComponent::RequestOpinionUpdate(ANPCCharacter* _about)
{
	if (!m_npc || !_about) return;
	if (!m_opinionRequestID.IsEmpty()) return;   

	ALLMRequestManager* mgr = ALLMRequestManager::Get();
	if (!mgr || !ALLMRequestManager::AreServersReady(m_npc->GetWorld()))
		return;   // best-effort: bei nicht bereiten Servern später erneut ausgelöst

	const FString prompt = BuildOpinionPrompt(_about);
	if (prompt.IsEmpty()) return;

	mgr->OnResponseReceived.AddUniqueDynamic(this, &UNPCBrainComponent::OnOpinionResponse);

	FLLMRequest req;
	req.RequestID    = FGuid::NewGuid().ToString();
	req.NPCName      = m_npc->GetNPCName();
	req.SystemPrompt = TEXT("You distill a stable opinion of a person from memories. Write all text in English only. Respond ONLY with the requested JSON.");
	req.UserMessage  = prompt;
	req.MaxTokens    = 120;
	req.JsonSchema   = LLMSchemas::Opinion();
	req.Priority     = static_cast<int32>(ELLMPriority::Low);
	req.Timestamp    = m_npc->GetWorld() ? m_npc->GetWorld()->GetTimeSeconds() : 0.0f;

	m_opinionRequestID = req.RequestID;
	m_opinionAbout     = _about;
	mgr->SubmitRequest(req);

}

void UNPCBrainComponent::OnOpinionResponse(const FLLMResponse& _response, bool _bSuccess)
{
	if (_response.RequestID != m_opinionRequestID) return;

	if (ALLMRequestManager* mgr = ALLMRequestManager::Get())
		mgr->OnResponseReceived.RemoveDynamic(this, &UNPCBrainComponent::OnOpinionResponse);
	m_opinionRequestID.Empty();

	ANPCCharacter* about = m_opinionAbout.Get();
	m_opinionAbout.Reset();
	if (!m_npc || !about || !_bSuccess) return;

	FString json = _response.Content.TrimStartAndEnd();
	if (!json.StartsWith(TEXT("{"))) json = TEXT("{") + json;
	int32 lastBrace = INDEX_NONE;
	if (json.FindLastChar('}', lastBrace)) json = json.Left(lastBrace + 1);

	TSharedPtr<FJsonObject> root;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(json);
	if (!FJsonSerializer::Deserialize(reader, root) || !root.IsValid()) return;

	FString opinion;
	root->TryGetStringField(TEXT("opinion"), opinion);
	opinion = opinion.TrimStartAndEnd();
	if (opinion.IsEmpty()) return;

	if (FRelationshipData* rel = m_npc->GetRelationships())
	{
		FNPCRelation& r = rel->GetOrAddRelation(about->GetEntityID());
		r.Opinion              = opinion;
		r.MemoriesSinceOpinion = 0;   // Zähler zurücksetzen
		UE_LOG(LogTemp, Warning, TEXT("[%s] Meinung über %s: %s"),
			   *m_npc->GetNPCName(), *about->GetNPCName(), *opinion);
	}
}


void UNPCBrainComponent::RequestGreeting()
{
	if (!m_npc) return;


	if (!m_greetingRequestID.IsEmpty())   return;
	if (m_npc->HasPendingGreeting())      return;

	const UWorld* world = m_npc->GetWorld();
	const float now = world ? world->GetTimeSeconds() : 0.0f;
	const float GreetingCooldownSeconds = 30.0f;   
	if (now - m_lastGreetingRealTime < GreetingCooldownSeconds) return;

	ALLMRequestManager* mgr = ALLMRequestManager::Get();
	if (!mgr || !ALLMRequestManager::AreServersReady(world)) return;

	const FString prompt = BuildGreetingPrompt();
	if (prompt.IsEmpty()) return;

	mgr->OnResponseReceived.AddUniqueDynamic(this, &UNPCBrainComponent::OnGreetingResponse);

	FLLMRequest req;
	req.RequestID    = FGuid::NewGuid().ToString();
	req.NPCName      = m_npc->GetNPCName();
	req.SystemPrompt = TEXT("You greet the player with one short in-character line, in English only. Respond ONLY with the requested JSON.");
	req.UserMessage  = prompt;
	req.MaxTokens    = 80;
	req.JsonSchema   = LLMSchemas::Greeting();
	req.Priority     = static_cast<int32>(ELLMPriority::Low);
	req.bPreferQwen  = true;   // NICHT über Hermes
	req.Timestamp    = now;

	m_greetingRequestID    = req.RequestID;
	m_lastGreetingRealTime = now;
	mgr->SubmitRequest(req);
}

void UNPCBrainComponent::OnGreetingResponse(const FLLMResponse& _response, bool _bSuccess)
{
	if (_response.RequestID != m_greetingRequestID) return;   

	if (ALLMRequestManager* mgr = ALLMRequestManager::Get())
		mgr->OnResponseReceived.RemoveDynamic(this, &UNPCBrainComponent::OnGreetingResponse);
	m_greetingRequestID.Empty();

	if (!m_npc || !_bSuccess) return;

	const FString reply = ParseLLMResponse(_response.Content).Reply.TrimStartAndEnd();
	if (!reply.IsEmpty())
	{
		m_npc->SetPendingGreeting(reply);
	}
}

FString UNPCBrainComponent::BuildGreetingPrompt() const
{
	if (!m_npc) return FString();
	const FString me   = m_npc->GetNPCName();
	const FString role = m_npc->NPCData ? m_npc->NPCData->GetOccupationString() : TEXT("villager");

	// Hat der Spieler eine offene Quest von DIESEM NPC? Dann eröffnet er das Gespräch,
	// indem er nach dem Fortschritt fragt (statt einer allgemeinen Begrüßung).
	FString questLine;
	if (const UWorld* w = m_npc->GetWorld())
		if (const APlayerController* pc = w->GetFirstPlayerController())
			if (const ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(pc->GetPawn()))
				for (const FQuestData& q : player->GetActiveQuests())
				{
					if (q.GiverName != me) continue;
					if (q.Type == EQuestType::Delivery)
						questLine = FString::Printf(
							TEXT("You earlier asked the player to take a message to %s."), *q.TargetNpc);
					else
						questLine = FString::Printf(
							TEXT("You earlier asked the player to bring you %d x %s."),
							q.TargetCount, *q.TargetItem.ToString());
					break;
				}

	FString prompt;
	prompt += FString::Printf(TEXT("You are %s, a %s in a medieval village.\n"), *me, *role);
	prompt += TEXT("The player just walked up to you. You speak first with ONE short, in-character sentence in English.\n");
	if (!questLine.IsEmpty())
	{
		prompt += questLine;
		prompt += TEXT(" Greet them and ask whether they have made progress on it. Do NOT give a new quest.\n");
	}
	else
	{
		prompt += TEXT("Greet them warmly. You may hint that you could use some help, but do NOT give a task or quest here.\n");
	}
	prompt += TEXT("Respond with ONLY this JSON on a single line: {\"reply\":\"your one sentence\"}");
	return prompt;
}
