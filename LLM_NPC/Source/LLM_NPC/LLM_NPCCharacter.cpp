// Copyright Epic Games, Inc. All Rights Reserved.

#include "LLM_NPCCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Components/SlateWrapperTypes.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"
#include "NPC/NPCCharacter.h"
#include "LLM_NPCGameMode.h"
#include "Blueprint/UserWidget.h"
#include "UI/ChatWidget.h"
#include "UI/ClockWidget.h"
#include "UI/ServerStatusWidget.h"
#include "UI/QuestToastWidget.h"
#include "UI/NpcQuestBoardWidget.h"
#include "LLM_NPC/LLM_Server/LLMRequestManager.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"                       // TActorIterator (Geber-NPC suchen)

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ALLM_NPCCharacter

ALLM_NPCCharacter::ALLM_NPCCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom 
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Test-Startausrüstung, damit das Inventar nicht leer ist.
	StartingItems.Add(TEXT("bread"), 3);
	StartingItems.Add(TEXT("herb"),  2);
	StartingItems.Add(TEXT("ore"),   1);
}

void ALLM_NPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	m_inventory.Gold = StartingGold;
	for (const TPair<FName, int32>& entry : StartingItems)
		if (entry.Value > 0)
			m_inventory.AddItem(entry.Key, entry.Value);

	for (const TPair<FName, int32>& pair : m_inventory.Items)
		UE_LOG(LogTemp, Warning, TEXT("[Inventory]    %s x%d"), *pair.Key.ToString(), pair.Value);

	APlayerController* PC = Cast<APlayerController>(GetController());
	
	if (ChatWidgetClass)
	{
		if (PC)
		{
			m_chatWidgetInstance = CreateWidget<UChatWidget>(PC, ChatWidgetClass);
			if (m_chatWidgetInstance)
			{
				//m_chatWidgetInstance->AddToViewport(10);
				//m_bChatVisible = true;
//
				//FInputModeGameAndUI InputMode;
				//InputMode.SetWidgetToFocus(m_chatWidgetInstance->TakeWidget());
				//InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				//PC->SetInputMode(InputMode);
				//PC->bShowMouseCursor = true;
				
				m_chatWidgetInstance->AddToViewport(10);
				if (UWidget* chatRoot = m_chatWidgetInstance->GetRootWidget())
					chatRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

				m_chatWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
				m_bChatVisible = false;
				
				PC->SetInputMode(FInputModeGameOnly());
				PC->bShowMouseCursor = false;
			}
		}
	}
	
	if (ServerStatusWidgetClass)
	{
		m_serverStatusInstance = CreateWidget<UServerStatusWidget>(PC, ServerStatusWidgetClass);
		if (m_serverStatusInstance)
			m_serverStatusInstance->AddToViewport(0);   
	}
	
	if (ClockWidgetClass)
	{
		m_clockWidget = CreateWidget<UClockWidget>(PC, ClockWidgetClass);
		if (m_clockWidget)
			m_clockWidget->AddToViewport(0);
	}

	// "Neue Quest"-Toast: automatisch ins Viewport, bleibt versteckt bis eine Quest kommt.
	if (QuestToastWidgetClass)
	{
		m_questToast = CreateWidget<UQuestToastWidget>(PC, QuestToastWidgetClass);
		if (m_questToast)
			m_questToast->AddToViewport(20);
	}

	// TEST: Beispiel-Quests 
	if (bSpawnTestQuests)
		SpawnTestQuests();
}

void ALLM_NPCCharacter::ToggleChat()
{
	if (!m_chatWidgetInstance) return;


	if (!m_bChatVisible)
	{
		
		if (m_bInventoryVisible || m_bNpcInventoryVisible) return;

		AActor* target = GetNearbyInteractable();
		IInteractableInterface* interactable = target ? Cast<IInteractableInterface>(target) : nullptr;
		if (!interactable || !interactable->CanInteract())
		{
			return;   
		}

		// Nicht-Chat-Interactable (z.B. Marktstand, Tür) → direkt behandeln, KEIN Chat.
		if (!interactable->UsesChatUI())
		{
			interactable->OnInteract(this);
			return;
		}

		// Solange die Server starten, kein Chat (es käme eh keine Antwort).
		if (!ALLMRequestManager::AreServersReady(GetWorld()))
			return;
	}

	m_bChatVisible = !m_bChatVisible;

	if (m_bChatVisible)
	{
		m_chatWidgetInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(m_chatWidgetInstance->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false); 
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
		}

		Interact();
	}
	else
	{
		// Gespräch beenden → NPC verlässt den Talk-State und nimmt den Ablauf wieder auf
		if (ANPCCharacter* npc = m_chatWidgetInstance->GetActiveNPC())
			npc->SetTalking(false);

		// NPC-Inventar wieder ausblenden.
		SetNpcInventoryVisible(false);
		SetNpcQuestBoardVisible(false);

		m_chatWidgetInstance->SetVisibility(ESlateVisibility::Hidden);

		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
	}
}

void ALLM_NPCCharacter::ToggleInventory()
{
	if (m_bChatVisible) return;          
	if (m_bShopOpen) return;             
	if (!InventoryWidgetClass) return;   

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (!m_inventoryWidget)
	{
		m_inventoryWidget = CreateWidget<UUserWidget>(PC, InventoryWidgetClass);
		if (!m_inventoryWidget) return;
		m_inventoryWidget->AddToViewport(5);
		m_inventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	m_bInventoryVisible = !m_bInventoryVisible;
	m_inventoryWidget->SetVisibility(
		m_bInventoryVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	RefreshInventoryInputMode();
}

void ALLM_NPCCharacter::ToggleQuestLog()
{
	if (m_bChatVisible) return;         
	if (m_bShopOpen) return;           
	if (!QuestLogWidgetClass) return;    

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (!m_questLogWidget)
	{
		m_questLogWidget = CreateWidget<UUserWidget>(PC, QuestLogWidgetClass);
		if (!m_questLogWidget) return;
		m_questLogWidget->AddToViewport(6);
		m_questLogWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	m_bQuestLogVisible = !m_bQuestLogVisible;
	m_questLogWidget->SetVisibility(
		m_bQuestLogVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);


	RefreshInventoryInputMode();
}

void ALLM_NPCCharacter::ToggleNpcInventory()
{
	if (m_bChatVisible) return;            
	if (m_bShopOpen) return;             
	if (!NpcInventoryWidgetClass) return;

	// Nur öffnen, wenn ein NPC anvisiert ist (das Panel liest GetTargetedNPC()).
	if (!m_bNpcInventoryVisible && !GetTargetedNPC())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	SetNpcInventoryVisible(!m_bNpcInventoryVisible);
}

void ALLM_NPCCharacter::SetNpcInventoryVisible(bool _bVisible)
{
	if (_bVisible == m_bNpcInventoryVisible) return;  
	if (_bVisible && !NpcInventoryWidgetClass) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;


	if (!m_npcInventoryWidget)
	{
		if (!_bVisible) return;   
		m_npcInventoryWidget = CreateWidget<UUserWidget>(PC, NpcInventoryWidgetClass);
		if (!m_npcInventoryWidget) return;
	
		m_npcInventoryWidget->AddToViewport(5);
		m_npcInventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	m_bNpcInventoryVisible = _bVisible;
	m_npcInventoryWidget->SetVisibility(
		_bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	RefreshInventoryInputMode();
}

void ALLM_NPCCharacter::SetNpcQuestBoardVisible(bool _bVisible)
{
	if (_bVisible == m_bNpcQuestBoardVisible) return; 
	if (_bVisible && !NpcQuestBoardWidgetClass) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (!m_npcQuestBoard)
	{
		if (!_bVisible) return;
		m_npcQuestBoard = CreateWidget<UNpcQuestBoardWidget>(PC, NpcQuestBoardWidgetClass);
		if (!m_npcQuestBoard) return;
		m_npcQuestBoard->AddToViewport(5);
		m_npcQuestBoard->SetVisibility(ESlateVisibility::Collapsed);
	}

	m_bNpcQuestBoardVisible = _bVisible;
	m_npcQuestBoard->SetVisibility(
		_bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (_bVisible)
		m_npcQuestBoard->Refresh();
}

void ALLM_NPCCharacter::ShowNpcQuestBoard()
{

	if (m_chatWidgetInstance)
		m_chatWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
	SetNpcInventoryVisible(false);
	SetNpcQuestBoardVisible(true);   
}

void ALLM_NPCCharacter::ShowNpcInventoryOnly()
{
	if (m_chatWidgetInstance)
		m_chatWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
	SetNpcQuestBoardVisible(false);
	SetNpcInventoryVisible(true);
}

void ALLM_NPCCharacter::BackToChatView()
{
	SetNpcQuestBoardVisible(false);
	SetNpcInventoryVisible(false);
	if (m_chatWidgetInstance)
		m_chatWidgetInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void ALLM_NPCCharacter::PostNpcChatLine(const FString& _text)
{
	if (m_chatWidgetInstance)
		m_chatWidgetInstance->AddNpcMessage(_text);
}

void ALLM_NPCCharacter::RefreshInventoryInputMode()
{
	if (m_bChatVisible) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (m_bInventoryVisible || m_bNpcInventoryVisible || m_bQuestLogVisible)
	{
		FInputModeGameAndUI mode;
		mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		mode.SetHideCursorDuringCapture(false);   
		PC->SetInputMode(mode);
		PC->bShowMouseCursor = true;
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ALLM_NPCCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ALLM_NPCCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ALLM_NPCCharacter::DoJump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALLM_NPCCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALLM_NPCCharacter::Look);
		
		//Interact
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ALLM_NPCCharacter::ToggleChat);
		//EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started,this, &ALLM_NPCCharacter::Interact);

		// Inventar öffnen/schließen
		if (InventoryAction)
			EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &ALLM_NPCCharacter::ToggleInventory);

		// NPC-Inventar (des anvisierten NPCs) öffnen/schließen
		if (NpcInventoryAction)
			EnhancedInputComponent->BindAction(NpcInventoryAction, ETriggerEvent::Started, this, &ALLM_NPCCharacter::ToggleNpcInventory);

		// Quest-Log öffnen/schließen
		if (QuestLogAction)
			EnhancedInputComponent->BindAction(QuestLogAction, ETriggerEvent::Started, this, &ALLM_NPCCharacter::ToggleQuestLog);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ALLM_NPCCharacter::AddNearbyInteractable(AActor* _actor)
{
	if (_actor)
		m_nearbyInteractables.AddUnique(_actor);
}

void ALLM_NPCCharacter::RemoveNearbyInteractable(AActor* _actor)
{
	m_nearbyInteractables.RemoveAll([_actor](const TWeakObjectPtr<AActor>& _p)
		{ return !_p.IsValid() || _p.Get() == _actor; });
}

void ALLM_NPCCharacter::PlayDoorGesture()
{
	if (!DoorGestureMontage) return;

	const float len = PlayAnimMontage(DoorGestureMontage);

	if (len <= 0.0f) return;

	m_bDoorGestureLock = true;
	if (UCharacterMovementComponent* move = GetCharacterMovement())
		move->StopMovementImmediately();

	GetWorldTimerManager().SetTimer(m_doorGestureTimer, this,
		&ALLM_NPCCharacter::EndDoorGesture, len, false);
}

void ALLM_NPCCharacter::EndDoorGesture()
{
	m_bDoorGestureLock = false;
}

AActor* ALLM_NPCCharacter::GetNearbyInteractable() const
{
	const FVector eye = FollowCamera ? FollowCamera->GetComponentLocation() : GetActorLocation();
	FVector fwd = FollowCamera ? FollowCamera->GetForwardVector() : GetActorForwardVector();
	fwd.Z = 0.0f;
	fwd.Normalize();

	AActor* best    = nullptr;
	float   bestDot = -2.0f;
	for (const TWeakObjectPtr<AActor>& p : m_nearbyInteractables)
	{
		AActor* a = p.Get();
		if (!a) continue;

		FVector dir = a->GetActorLocation() - eye;
		dir.Z = 0.0f;
		if (!dir.Normalize()) continue;

		const float dot = FVector::DotProduct(fwd, dir);
		if (dot > bestDot)
		{
			bestDot = dot;
			best    = a;
		}
	}

	// Nur anvisieren, wenn der NPC im Sichtkegel liegt
	return (bestDot >= InteractAimThreshold) ? best : nullptr;
}

void ALLM_NPCCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ALLMRequestManager::AreServersReady(GetWorld()))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(424242, 0.5f, FColor::Yellow,
				TEXT("LLM-Server starten ... bitte warten"));
		return;   
	}

	UpdateHighlight();
}

void ALLM_NPCCharacter::UpdateHighlight()
{
	AActor* target = GetNearbyInteractable();
	if (target == m_highlighted.Get()) return;   

	if (ANPCCharacter* oldNpc = Cast<ANPCCharacter>(m_highlighted.Get()))
		oldNpc->SetHighlighted(false);
	if (ANPCCharacter* newNpc = Cast<ANPCCharacter>(target))
		newNpc->SetHighlighted(true);

	m_highlighted = target;
}

void ALLM_NPCCharacter::Interact()
{
	AActor* interactableActor = GetNearbyInteractable();
	if (!interactableActor || !m_chatWidgetInstance) return;

	IInteractableInterface* interactable = Cast<IInteractableInterface>(interactableActor);
	if (!interactable || !interactable->CanInteract()) return;

	interactable->OnInteract(this);

	FString prompt = interactable->GetInteractionPrompt();
	if (!prompt.IsEmpty())
	{
		ANPCCharacter* npc = Cast<ANPCCharacter>(interactableActor);
		m_chatWidgetInstance->OpenForNPC(prompt, npc);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Player] Interagiert mit %s"),
		   *interactableActor->GetActorNameOrLabel());
}

void ALLM_NPCCharacter::StartChatWith(ANPCCharacter* _npc)
{
	if (!_npc || !m_chatWidgetInstance) return;
	if (!ALLMRequestManager::AreServersReady(GetWorld())) return;   

	m_bChatVisible = true;
	m_chatWidgetInstance->SetVisibility(ESlateVisibility::Visible);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(m_chatWidgetInstance->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);   
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	if (IInteractableInterface* it = Cast<IInteractableInterface>(_npc))
		it->OnInteract(this);

	m_chatWidgetInstance->OpenForNPC(_npc->GetInteractionPrompt(), _npc);
}

// ============================================================================
//  Quests (vom LLM im Dialog erzeugt)
// ============================================================================
void ALLM_NPCCharacter::AddQuest(const FQuestData& _quest)
{
	for (const FQuestData& q : m_activeQuests)
	{
		if (q.GiverName != _quest.GiverName) continue;
		const bool bSameFetch = _quest.Type == EQuestType::Fetch && q.Type == EQuestType::Fetch
			&& !_quest.TargetItem.IsNone() && q.TargetItem == _quest.TargetItem;
		const bool bSameDelivery = _quest.Type == EQuestType::Delivery && q.Type == EQuestType::Delivery
			&& !_quest.TargetNpc.IsEmpty() && q.TargetNpc.Equals(_quest.TargetNpc, ESearchCase::IgnoreCase);
		if (bSameFetch || bSameDelivery)
		{
			return;
		}
	}

	m_activeQuests.Add(_quest);


	// UI benachrichtigen: kurze Einblendung + Quest-Log aktualisieren.
	OnQuestAdded.Broadcast(_quest);
	OnQuestsChanged.Broadcast();
}

void ALLM_NPCCharacter::SpawnTestQuests()
{
	auto Make = [](const FString& _id, const FString& _giver, const FString& _title,
	               const FString& _desc, FName _item, int32 _count, int32 _reward)
	{
		FQuestData q;
		q.QuestId     = _id;
		q.GiverName   = _giver;
		q.Title       = _title;
		q.Description = _desc;
		q.TargetItem  = _item;
		q.TargetCount = _count;
		q.RewardGold  = _reward;
		return q;
	};

	m_activeQuests.Add(Make(TEXT("q_test_bram"),   TEXT("Old Bram"),
		TEXT("Fresh Grain"),          TEXT("Old Bram needs wheat for the mill."),  TEXT("wheat"), 3, 15));
	m_activeQuests.Add(Make(TEXT("q_test_elsy"),   TEXT("Elsy"),
		TEXT("Eggs for the Bakery"),  TEXT("Elsy needs eggs for baking."),         TEXT("egg"),   5, 20));
	m_activeQuests.Add(Make(TEXT("q_test_magnus"), TEXT("Magnus the Magnificent"),
		TEXT("Ore for the Trader"),   TEXT("Magnus pays well for iron ore."),      TEXT("ore"),   2, 25));

	for (const FQuestData& q : m_activeQuests)
	{
		if (q.TargetItem.IsNone() || q.TargetCount <= 0) continue;
		m_inventory.AddItem(q.TargetItem, q.TargetCount);
	}

	OnQuestsChanged.Broadcast();
}

int32 ALLM_NPCCharacter::TryCompleteQuestsFrom(const FString& _giverName)
{
	int32 completed = 0;
	for (int32 i = m_activeQuests.Num() - 1; i >= 0; --i)
	{
		FQuestData& q = m_activeQuests[i];
		if (q.GiverName != _giverName)          continue;
		if (q.TargetItem.IsNone() || q.TargetCount <= 0) continue;
		if (!m_inventory.HasItem(q.TargetItem, q.TargetCount)) continue;

		m_inventory.RemoveItem(q.TargetItem, q.TargetCount);
		m_inventory.AddGold(q.RewardGold);
		m_activeQuests.RemoveAt(i);
		++completed;
	}

	if (completed > 0)
		OnQuestsChanged.Broadcast();

	return completed;
}

bool ALLM_NPCCharacter::TryCompleteQuest(const FString& _questId)
{
	for (int32 i = 0; i < m_activeQuests.Num(); ++i)
	{
		FQuestData& q = m_activeQuests[i];
		if (q.QuestId != _questId)                          continue;
		if (q.TargetItem.IsNone() || q.TargetCount <= 0)    return false;
		if (!m_inventory.HasItem(q.TargetItem, q.TargetCount)) return false;   

		m_inventory.RemoveItem(q.TargetItem, q.TargetCount);
		m_inventory.AddGold(q.RewardGold);

		m_activeQuests.RemoveAt(i);
		OnQuestsChanged.Broadcast();
		return true;
	}
	return false;
}

bool ALLM_NPCCharacter::DeliverMessageQuest(const FString& _questId)
{

	int32 idx = INDEX_NONE;
	for (int32 i = 0; i < m_activeQuests.Num(); ++i)
		if (m_activeQuests[i].QuestId == _questId) { idx = i; break; }
	if (idx == INDEX_NONE) return false;

	const FQuestData q = m_activeQuests[idx];
	if (q.Type != EQuestType::Delivery) return false;


	ANPCCharacter* target = GetTargetedNPC();
	if (!target || !q.TargetNpc.Equals(target->GetNPCName(), ESearchCase::IgnoreCase))
	{
		return false;
	}

	// Absender (A) im Level suchen – für Reflexion/Meinung des Ziel-NPCs über ihn.
	ANPCCharacter* giver = nullptr;
	if (const UWorld* w = GetWorld())
		for (TActorIterator<ANPCCharacter> it(w); it; ++it)
			if (*it && (*it)->GetNPCName().Equals(q.GiverName, ESearchCase::IgnoreCase)) { giver = *it; break; }

	// Quest einlösen: Belohnung + aus dem Log nehmen.
	m_inventory.AddGold(q.RewardGold);
	m_activeQuests.RemoveAt(idx);
	OnQuestsChanged.Broadcast();

	const FString learned = FString::Printf(TEXT("%s sent word through a messenger: %s"),
		*q.GiverName, *q.Description);
	target->OnLearnedInformation(learned, 0.7f, giver, /*bReplan*/ false);

	// Reflexion/Meinung gezielt am Absender A verankern (Beziehungs-Erinnerung).
	if (giver)
		if (FRelationshipData* rel = target->GetRelationships())
			rel->AddMemory(giver->GetEntityID(), giver->GetNPCName(),
				FString::Printf(TEXT("%s sent me a message via the player: %s"), *q.GiverName, *q.Description));

	// Der Ziel-NPC antwortet per LLM im Chat-Fenster
	if (m_chatWidgetInstance)
		m_chatWidgetInstance->TriggerDeliveryReply(q.GiverName, q.Description);
	BackToChatView();

	return true;
}

TArray<FInventorySlot> ALLM_NPCCharacter::GetInventorySlots() const
{
	if (const UWorld* w = GetWorld())
		if (const ALLM_NPCGameMode* gm = w->GetAuthGameMode<ALLM_NPCGameMode>())
			return gm->MakeSlots(m_inventory);

	TArray<FInventorySlot> slots;
	for (const TPair<FName, int32>& pair : m_inventory.Items)
	{
		FInventorySlot slot;
		slot.ItemId      = pair.Key;
		slot.Count       = pair.Value;
		slot.DisplayName = pair.Key.ToString();
		slots.Add(slot);
	}
	return slots;
}

ANPCCharacter* ALLM_NPCCharacter::GetTargetedNPC() const
{
	return Cast<ANPCCharacter>(GetNearbyInteractable());
}

void ALLM_NPCCharacter::DoJump()
{
	if (m_bChatVisible || !ALLMRequestManager::AreServersReady(GetWorld())) return;
	Jump();
}

void ALLM_NPCCharacter::Move(const FInputActionValue& Value)
{
	if (m_bChatVisible || m_bInventoryVisible || m_bNpcInventoryVisible || m_bQuestLogVisible ||
		m_bShopOpen || m_bDoorGestureLock || !ALLMRequestManager::AreServersReady(GetWorld())) return;

	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ALLM_NPCCharacter::Look(const FInputActionValue& Value)
{
	if (m_bChatVisible || m_bInventoryVisible || m_bNpcInventoryVisible || m_bQuestLogVisible || m_bShopOpen) return;

	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
