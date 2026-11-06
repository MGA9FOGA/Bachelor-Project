// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "LLM_NPC/Economy/InventoryData.h"
#include "LLM_NPC/Economy/InventorySlot.h"
#include "LLM_NPC/Economy/QuestData.h"
#include "LLM_NPCCharacter.generated.h"

class ANPCCharacter;

class UChatWidget;
class UUserWidget;
class UAnimMontage;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestAdded, const FQuestData&, NewQuest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQuestsChanged);

UCLASS(config=Game)
class ALLM_NPCCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* InteractAction;

	// Inventar öffnen/schließen (eigene Input-Action, z.B. Taste I).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* InventoryAction;

	// Inventar des ANVISIERTEN NPCs öffnen/schließen (eigene Taste, z.B. T).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* NpcInventoryAction;

	// Quest-Log öffnen/schließen (eigene Input-Action, z.B. Taste J).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* QuestLogAction;


public:
	ALLM_NPCCharacter();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UChatWidget> ChatWidgetClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	TSubclassOf<class UServerStatusWidget> ServerStatusWidgetClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	TSubclassOf<class UClockWidget> ClockWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	TSubclassOf<class UQuestToastWidget> QuestToastWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	TSubclassOf<UUserWidget> InventoryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	TSubclassOf<UUserWidget> NpcInventoryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	TSubclassOf<UUserWidget> QuestLogWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	TSubclassOf<class UNpcQuestBoardWidget> NpcQuestBoardWidgetClass;

	// Sichtkegel fürs Anvisieren: cos des halben Öffnungswinkels.
	// 0.85 ≈ 32°, 0.95 ≈ 18° (enger). Höher = dichter/strenger.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	float InteractAimThreshold = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
	TObjectPtr<UAnimMontage> DoorGestureMontage;

	void PlayDoorGesture();
	
	void AddNearbyInteractable(AActor* _actor);
	void RemoveNearbyInteractable(AActor* _actor);
	AActor* GetNearbyInteractable() const;
	
	UChatWidget* GetChatWidget() const { return m_chatWidgetInstance; }

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void StartChatWith(ANPCCharacter* _npc);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetShopOpen(bool _bOpen) { m_bShopOpen = _bOpen; }

	// ========== WIRTSCHAFT (Inventar + Gold) ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	int32 StartingGold = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	TMap<FName, int32> StartingItems;

	UFUNCTION(BlueprintCallable, Category="Economy")
	int32 GetGold() const { return m_inventory.Gold; }

	UFUNCTION(BlueprintCallable, Category="Economy")
	void AddGold(int32 _amount) { m_inventory.AddGold(_amount); }

	UFUNCTION(BlueprintCallable, Category="Economy")
	bool SpendGold(int32 _amount) { return m_inventory.SpendGold(_amount); }

	UFUNCTION(BlueprintCallable, Category="Economy")
	void AddItem(FName _itemId, int32 _count = 1) { m_inventory.AddItem(_itemId, _count); }

	UFUNCTION(BlueprintCallable, Category="Economy")
	bool RemoveItem(FName _itemId, int32 _count = 1) { return m_inventory.RemoveItem(_itemId, _count); }

	UFUNCTION(BlueprintCallable, Category="Economy")
	int32 GetItemCount(FName _itemId) const { return m_inventory.GetItemCount(_itemId); }

	UFUNCTION(BlueprintPure, Category="Economy")
	TArray<FInventorySlot> GetInventorySlots() const;

	UFUNCTION(BlueprintPure, Category="Interaction")
	ANPCCharacter* GetTargetedNPC() const;

	// ========== QUESTS (vom LLM erzeugt) ==========
	UFUNCTION(BlueprintCallable, Category="Quests")
	void AddQuest(const FQuestData& _quest);

	UFUNCTION(BlueprintCallable, Category="Quests")
	int32 TryCompleteQuestsFrom(const FString& _giverName);

	UFUNCTION(BlueprintCallable, Category="Quests")
	bool TryCompleteQuest(const FString& _questId);

	UFUNCTION(BlueprintCallable, Category="Quests")
	bool DeliverMessageQuest(const FString& _questId);

	UFUNCTION(BlueprintCallable, Category="UI")
	void ShowNpcQuestBoard();      // nur das Quest-Board zeigen

	UFUNCTION(BlueprintCallable, Category="UI")
	void ShowNpcInventoryOnly();   // nur das NPC-Inventar zeigen

	UFUNCTION(BlueprintCallable, Category="UI")
	void BackToChatView();        


	UFUNCTION(BlueprintCallable, Category="UI")
	void PostNpcChatLine(const FString& _text);

	UFUNCTION(BlueprintPure, Category="Quests")
	TArray<FQuestData> GetActiveQuests() const { return m_activeQuests; }

	UFUNCTION(BlueprintPure, Category="Quests")
	int32 GetActiveQuestCount() const { return m_activeQuests.Num(); }

	// Neue Quest erhalten – für eine kurze Einblendung/Toast.
	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestAdded OnQuestAdded;

	// Quest-Liste hat sich geändert – Quest-Log baut sich darauf neu auf.
	UPROPERTY(BlueprintAssignable, Category="Quests")
	FOnQuestsChanged OnQuestsChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quests")
	bool bSpawnTestQuests = true;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Springen – blockiert während der Chat offen ist */
	void DoJump();

	void ToggleChat();
	void ToggleInventory();

	void ToggleNpcInventory();

	void ToggleQuestLog();

protected:

	virtual void NotifyControllerChanged() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
private:
	UPROPERTY()
	UChatWidget* m_chatWidgetInstance;
	
	// Alle NPCs in Reichweite; das aktive Ziel ist der, den die Kamera anvisiert
	TArray<TWeakObjectPtr<AActor>> m_nearbyInteractables;

	// Aktuell gehighlighteter (anvisierter) NPC
	TWeakObjectPtr<AActor> m_highlighted;

	// Aktualisiert das Highlight auf den anvisierten NPC
	void UpdateHighlight();

	void Interact();

	bool m_bChatVisible = false;

	// Sperrt die Bewegung, solange die Tür-Geste läuft (siehe PlayDoorGesture).
	bool m_bDoorGestureLock = false;
	FTimerHandle m_doorGestureTimer;

	// Gibt die Bewegung nach Ende der Tür-Geste wieder frei.
	void EndDoorGesture();

	// Inventar-Widget-Instanzen (lazy erstellt) + Sichtbarkeit.
	UPROPERTY()
	UUserWidget* m_inventoryWidget = nullptr;
	bool m_bInventoryVisible = false;

	UPROPERTY()
	UUserWidget* m_npcInventoryWidget = nullptr;
	bool m_bNpcInventoryVisible = false;

	UPROPERTY()
	class UNpcQuestBoardWidget* m_npcQuestBoard = nullptr;
	bool m_bNpcQuestBoardVisible = false;

	UPROPERTY()
	UUserWidget* m_questLogWidget = nullptr;
	bool m_bQuestLogVisible = false;

	bool m_bShopOpen = false;

	void RefreshInventoryInputMode();

	void SpawnTestQuests();

	void SetNpcInventoryVisible(bool _bVisible);

	void SetNpcQuestBoardVisible(bool _bVisible);

	UPROPERTY()
	UServerStatusWidget* m_serverStatusInstance = nullptr;

	UPROPERTY()
	UClockWidget* m_clockWidget = nullptr;

	UPROPERTY()
	class UQuestToastWidget* m_questToast = nullptr;

	FInventoryData m_inventory;

	TArray<FQuestData> m_activeQuests;
};

