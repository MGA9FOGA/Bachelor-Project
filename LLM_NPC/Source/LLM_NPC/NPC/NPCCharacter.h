#pragma once

#include "CoreMinimal.h"
#include "NPCDataAsset.h"
#include "GameFramework/Character.h"
#include "Components/NPC_ECS.h"
#include "Components/SphereComponent.h"
#include "LLM_NPC/Interface/InteractableInterface.h"
#include "LLM_NPC/World/WorldLocationDataAsset.h"
#include "LLM_NPC/Economy/InventorySlot.h"
#include "LLM_NPC/Economy/QuestData.h"
#include "NPCCharacter.generated.h"


/**
 * ============================================================================
 * NPC CHARACTER MIT EIGENSTÄNDIGEM ECS SYSTEM
 * ============================================================================
 *
 * Der Character hält ein eigenständiges ECS-System (FNPCECS)
 * und updatet es in jedem Frame.
 *
 */

struct FLLMResponse;
class UNPCDataAsset;
class UBehaviorTree;
class AWorldSpot;
class UAnimMontage;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UNPCBrainComponent;
class UWidgetComponent;
class UTextRenderComponent;
class UUserWidget;
class UPhysicalMaterial;
class FJsonValue;
class AMarketStall;
class UItemDataAsset;
enum class EConsumeType : uint8;

USTRUCT()
struct FNPCDialogResponse
{
	GENERATED_BODY()
    
	FString Reply;
	EIntentType Intent = EIntentType::Neutral;
	EEmotionType Mood  = EEmotionType::Neutral;
	bool bParseSuccess = false;


	bool bHasQuest = false;
	FQuestData Quest;
};

UCLASS(Abstract)
class LLM_NPC_API ANPCCharacter : public ACharacter, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ANPCCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	
	// DataAsset Character Personality Info
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC Data")
	TObjectPtr<UNPCDataAsset> NPCData;

	// Behavior Tree für diesen NPC 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UBehaviorTree* GetBehaviorTree() const { return BehaviorTreeAsset; }

	// Lässt das LLM beim Start einen Tagesablauf generieren (überschreibt den Start-Plan)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
	bool bUseLLMSchedule = true;

	// Radius (cm) um den NPC, in dem beim Umherwandern ein zufälliger NavMesh-Punkt
	// gesucht wird. 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
	float WanderRadius = 900.0f;

	// Physical Material des GRAS-Layers → wird beim Wandern gemieden (NPCs laufen
	// bevorzugt auf Wegen/Stein). Leer = kein Oberflächen-Filter (wandert überallhin).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
	TObjectPtr<UPhysicalMaterial> WanderAvoidSurface;

	// Wanderpunkte näher als dies am Marktplatz-Zentrum werden verworfen, 0 = aus.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin=0.0))
	float MarketAvoidRadius = 700.0f;

	bool FindWanderTarget(FVector& _out) const;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin=0.0, ClampMax=1.0))
	float WorkBusySocializeChance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin=0.0, ClampMax=1.0))
	float ScheduleReplanMinImportance = 0.6f;

	UFUNCTION(BlueprintCallable, Category="AI")
	void ReplanSchedule(bool _bForce = false);

	// Der NPC erfährt etwas Neues (vom Spieler im Gespräch oder von anderen NPCs):
	// merkt es sich und plant 
	UFUNCTION(BlueprintCallable, Category="AI")
	void OnLearnedInformation(const FString& _info, float _importance = 0.6f, AActor* _source = nullptr,
	                          bool _bReplan = true);

	// ===== PROAKTIVE BEGRÜSSUNG (NPC eröffnet das Gespräch) =====
	UFUNCTION(BlueprintCallable, Category="AI")
	void RequestGreeting();

	// Vom Gehirn gesetzt, sobald die Begrüßung fertig ist.
	void SetPendingGreeting(const FString& _line) { m_pendingGreeting = _line; }

	bool HasPendingGreeting() const { return !m_pendingGreeting.IsEmpty(); }

	FString ConsumePendingGreeting()
	{
		const FString g = m_pendingGreeting;
		m_pendingGreeting.Reset();
		return g;
	}
	
	UFUNCTION(BlueprintCallable, Category="NPC|Movement")
	void ChooseNextDestination();

	// ========== ECS ACCESS ==========

	// Getter des ECS-Systems

	FNPCECS* GetECS() { return &m_ecs; }

	const FNPCECS* GetECS() const { return &m_ecs; }

	//Getter Entity ID des NPCs
	EntityID GetEntityID() const { return m_entityID; }

	//Hole eine beliebeige Komponente vom ECS
	template <typename ComponentType>
	ComponentType* GetComponent()
	{
		return m_ecs.GetComponent<ComponentType>(m_entityID);
	}

	template <typename ComponentType>
	const ComponentType* GetComponent() const
	{
		return m_ecs.GetComponent<ComponentType>(m_entityID);
	}
	
	//Animations

	void PlaySpotAnimation(AWorldSpot* _spot);
	void PlaySpotOutro();
	bool IsPlayingOutro() const;
	void StopSpotAnimation();

	void ReleaseCurrentWorkstation();

	// Reservierter Spot (für BTTask_PerformAction: Drehung + Montage)
	AWorldSpot* GetReservedSpot() const { return m_reservedSpot.Get(); }

	// ========== MARKTSTAND (Verkäufer) ==========
	void SetAssignedStall(AMarketStall* _stall);
	AMarketStall* GetAssignedStall() const;

	bool TrySnapToStallStand();

	// ========== INTERACTABLE INTERFACE ==========
	virtual void OnInteract(AActor* _interactor) override;
	virtual FString GetInteractionPrompt() override;
	virtual bool CanInteract() const override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool IsPlayerInRange() const { return m_bPlayerInRange; }

	// Stencil-Wert fürs Outline-PP-Material 
	UPROPERTY(EditAnywhere, Category = "Interaction")
	int32 HighlightStencil = 2;

	// Outline ein/aus 
	void SetHighlighted(bool _bOn);

	// ========== KOMPONENTEN SHORTCUTS ==========
	
	FIdentityData* GetIdentity() { return GetComponent<FIdentityData>(); }
	
	FMemoryData* GetMemory() { return GetComponent<FMemoryData>(); }

	FReflectionData* GetReflection() { return GetComponent<FReflectionData>(); }

	FConversationData* GetConversation() { return GetComponent<FConversationData>(); }
	
	FEmotionData* GetEmotion() { return GetComponent<FEmotionData>(); }
	
	FRelationshipData* GetRelationships() { return GetComponent<FRelationshipData>(); }
	
	FScheduleData* GetSchedule() { return GetComponent<FScheduleData>(); }

	FDialogData* GetDialog() { return GetComponent<FDialogData>(); }
	
	FTransformData* GetTransform() { return GetComponent<FTransformData>(); }
	
	FNavMeshData* GetNavMesh() { return GetComponent<FNavMeshData>(); }

	FBTStateData* GetBTState() { return GetComponent<FBTStateData>(); }
	
	FNeedsData* GetNeeds() { return GetComponent<FNeedsData>(); }

	FWaypointData* GetWaypoints() { return GetComponent<FWaypointData>(); }

	FInventoryData* GetInventory() { return GetComponent<FInventoryData>(); }

	// ========== WIRTSCHAFT (Inventar + Gold) ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Economy")
	int32 StartingGold = 25;

	UFUNCTION(BlueprintCallable, Category="Economy")
	int32 GetGold() const;

	UFUNCTION(BlueprintCallable, Category="Economy")
	void AddGold(int32 _amount);

	UFUNCTION(BlueprintCallable, Category="Economy")
	bool SpendGold(int32 _amount);

	UFUNCTION(BlueprintCallable, Category="Economy")
	void AddItem(FName _itemId, int32 _count = 1);

	UFUNCTION(BlueprintCallable, Category="Economy")
	bool RemoveItem(FName _itemId, int32 _count = 1);

	UFUNCTION(BlueprintCallable, Category="Economy")
	int32 GetItemCount(FName _itemId) const;


	UFUNCTION(BlueprintPure, Category="Economy")
	TArray<FInventorySlot> GetInventorySlots() const;

	UItemDataAsset* FindConsumable(EConsumeType _type) const;

	bool ConsumeItem(FName _itemId);

	void TryConsumePending();

	// ========== DEBUG ==========
	UFUNCTION(BlueprintCallable, Category = "ECS|Debug")
	void  DebugPrintECS()
	{
		m_ecs.DebugPrintEntity(m_entityID);
	}

	// ========== HERMES PROMPT GENERATION ==========
	UFUNCTION(BlueprintCallable, Category = "ECS|Hermes")
	FString GenerateHermesPrompt();

	UFUNCTION(BlueprintCallable, Category = "ECS|Hermes")
	FString GenerateSchedulePrompt();

	FString BuildDialogueContext();
	FString BuildScheduleContext();

	UFUNCTION(BlueprintCallable, Category = "ECS|Hermes")
	FString GenerateMemoryPrompt();

	UFUNCTION(BlueprintCallable, Category = "ECS|Hermes")
	FString GenerateNeedsPrompt();

	UFUNCTION(BlueprintCallable, Category = "ECS|Hermes")
	FString GenerateEmotionPrompt();
	
	
	// ========== LLM RESPONSE ==========
	// Wird vom ChatWidget gezielt für genau diesen NPC aufgerufen
	void ApplyDialogueResult(const FString& _reply, EIntentType _intent, EEmotionType _mood);

	// Dialog-Verlauf (Laufzeit, pro NPC) – fürs Wiederanzeigen beim erneuten Ansprechen.
	void AddDialogTurn(const FString& _text, bool _bIsNPCResponse);

	FNPCDialogResponse ParseLLMResponse(const FString& RawResponse);

	// ========== INTERAKTION / TALK-STATE ==========
	void SetTalking(bool _bTalking, AActor* _interactor = nullptr);

	AActor* GetTalkTarget() const { return m_talkTarget.Get(); }

	// ========== NPC-ZU-NPC-GESPRÄCHE ==========
	void StartConversationWith(ANPCCharacter* _other);

	void EndConversation();

	bool IsInConversation() const { return m_talkTarget.IsValid(); }

	// Sprechblase: Dialogzeilen des laufenden NPC-Gesprächs 
	void SetConversationSubtitles(const TArray<FString>& _speakers, const TArray<FString>& _texts);

	void ShowSpeechLine(const FString& _text);
	void HideSpeechBubble();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
	TSubclassOf<UUserWidget> SpeechBubbleWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social")
	float SocialRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social")
	float SocialChatDuration = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social")
	float SocialCooldown = 45.0f;

	// NPC-zu-NPC-Gespräche global an/aus.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social")
	bool bEnableNPCConversations = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social")
	bool bAutoStartConversations = true;

rse.
	bool TryStartNPCConversation();

	// Stößt eine Reflexion an (async, best-effort). 
	void TriggerReflection();

	// Sympathie-Änderung pro Gespräch (Vertrauen ändert sich etwas langsamer).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social", meta=(ClampMin=0.0, ClampMax=0.5))
	float FriendshipGainPerChat = 0.06f;

	// ---- Sympathie ----
	// Sympathie dieses NPCs zum SPIELER (0..1, 0.5 = neutral). 
	UFUNCTION(BlueprintPure, Category="AI|Social")
	float GetPlayerLiking() const { return m_playerLiking; }

	UFUNCTION(BlueprintCallable, Category="AI|Social")
	void AdjustPlayerLiking(float _delta);

	// Sympathie dieses NPCs zu einer anderen Entität (0.5, falls keine Beziehung).
	float GetLikingToward(uint32 _entityId) const;

	// Verfall pro Spielstunde Richtung neutral (0.5), wenn nicht geredet wird.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social", meta=(ClampMin=0.0, ClampMax=0.2))
	float RelationshipDecayPerHour = 0.01f;

	// Schreibt jedes NPC-Gespräch als JSON nach Saved/NPCConversations/ 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social")
	bool bLogConversationsToJson = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social", meta=(ClampMin=0.0, ClampMax=1.0))
	float AvoidLikingThreshold = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Social")
	bool bLLMDrivenConversations = true;

	// === Talk-Animationen (pro NPC einstellbar) ===
	// TalkIdleMontage:  läuft das GANZE Gespräch (zuhören/gestikulieren) und wird
	//                   nur unterbrochen, während der NPC antwortet.
	// TalkReplyMontage: läuft, solange der NPC ANTWORTET (Typewriter), danach
	//                   zurück zur Rede-Idle. 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Talk")
	TObjectPtr<UAnimMontage> TalkIdleMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation|Talk")
	TObjectPtr<UAnimMontage> TalkReplyMontage;

	void PlayTalkReply();
	void EndTalkReply();

	// ========== TÜR-INTERAKTION ==========
	bool PauseMovementForDoor();

	void ResumeMovementFromDoor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> DoorGestureMontage;

	FString GetNPCName() const
	{
		return NPCData ? NPCData->NPCName : GetActorNameOrLabel();
	}
protected:
	void InitializeECS();

	void ApplyModularAppearance();

private:
	// ========== PRIVATE MEMBERS ==========

	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> m_outfitComponents;

	
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> m_attachmentComponents;

	FNPCECS m_ecs;
	EntityID m_entityID = INVALID_ENTITY;
	EWorldLocationTag m_currentLocationTag = EWorldLocationTag::None;

	TWeakObjectPtr<UAnimMontage> m_spotMontage;

	FTimerHandle m_loopActiveTimer;

	bool m_bOutroPlayed = false;

	void SetSpotLoopActive(bool _bActive);

	void PlayCurrentTalkMontage();
	void OnTalkMontageBlendingOut(UAnimMontage* _montage, bool _bInterrupted);
	void StopTalkAnimation();

	bool m_bTalkReplying = false;
	bool m_bTalkReplyEndPending = false;

	void ApplyIntroOffset(const FVector& _offset);
	void ClearIntroOffset();

	FVector m_appliedIntroOffset = FVector::ZeroVector;
	bool    m_bIntroOffsetActive = false;

	// Aktuell reservierter Spot-Actor (Bett/Amboss) – fürs Freigeben + Animation
	TWeakObjectPtr<AWorldSpot> m_reservedSpot;

	// Zugewiesener Marktstand (nur bei Verkäufer-NPCs gesetzt).
	TWeakObjectPtr<AMarketStall> m_assignedStall;

	TWeakObjectPtr<AMarketStall> m_reservedStall;

	float m_playerLiking = 0.5f;

	// Item, das am nächsten Rest-Platz verzehrt werden soll (leer = keins)
	FName m_pendingConsume = NAME_None;

	ENPCDesire m_lastGoal = ENPCDesire::None;


	TWeakObjectPtr<AActor> m_talkTarget;

	// ========== NPC-zu-NPC-Gespräche (State) ==========
	TWeakObjectPtr<ANPCCharacter> m_conversationPartner;
	float m_socialCooldown = 0.0f;
	bool  m_bConvoInitiatorSpeaks = false;
	FTimerHandle m_conversationEndTimer;
	FTimerHandle m_conversationGestureTimer;

	UPROPERTY()
	TObjectPtr<UWidgetComponent> m_speechBubble;

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TObjectPtr<UTextRenderComponent> m_nameTag;

	// Dialogzeilen des laufenden Gesprächs (Sprecher + Text) und der aktuelle Index.
	TArray<FString> m_convLineSpeakers;
	TArray<FString> m_convLineTexts;
	int32           m_convLineIndex = 0;

	// Frei für ein Gespräch? 
	bool CanStartConversation() const;
	// Sucht einen nahen, ebenfalls freien NPC als Gesprächspartner.
	ANPCCharacter* FindConversationPartner() const;
	// Beide teilen ihre wichtigste Erinnerung (Gerücht verbreitet sich, ohne Replan).
	void ExchangeGossip(ANPCCharacter* _other);

	void ConversationGestureTick();

	// ========== Interact ==========
	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	USphereComponent* m_interactionSphere;

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	USphereComponent* m_awarenessSphere;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float GreetingRange = 700.0f;

	bool m_bPlayerInRange = false;

	UFUNCTION()
	void OnInteractionSphereBeginOverlap(UPrimitiveComponent* _overlappedComp,
		AActor* _otherActor, UPrimitiveComponent* _otherComp,
		int32 _otherBodyIndex, bool _bFromSweep, const FHitResult& _sweepResult);

	UFUNCTION()
	void OnInteractionSphereEndOverlap(UPrimitiveComponent* _overlappedComp,
		AActor* _otherActor, UPrimitiveComponent* _otherComp,
		int32 _otherBodyIndex);

	UFUNCTION()
	void OnAwarenessSphereBeginOverlap(UPrimitiveComponent* _overlappedComp,
		AActor* _otherActor, UPrimitiveComponent* _otherComp,
		int32 _otherBodyIndex, bool _bFromSweep, const FHitResult& _sweepResult);
	
	void SyncGoalToBlackboard();
	void LoadTestSchedule();

	void RequestLLMSchedule(bool _bForce = false);

	UPROPERTY()
	TObjectPtr<UNPCBrainComponent> m_brain;

	// Letzter Spieltag, für den geplant wurde (tägliche Neuplanung)
	int32 m_lastScheduleDay = 0;

	FString m_pendingGreeting;
};
