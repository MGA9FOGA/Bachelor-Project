#include "Door.h"

#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "LLM_NPC/NPC/NPCCharacter.h"
#include "LLM_NPC/LLM_NPCCharacter.h"
#include "LLM_NPC/Audio/SoundManager.h"

ADoor::ADoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;  

	m_root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(m_root);

	m_hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	m_hinge->SetupAttachment(m_root);

	m_mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	m_mesh->SetupAttachment(m_hinge);

	// Blockiert den Spieler physisch, beeinflusst aber die KI-Navigation NICHT –
	m_mesh->SetCanEverAffectNavigation(false);

	m_trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	m_trigger->SetupAttachment(m_root);
	m_trigger->SetUsingAbsoluteScale(true);
	m_trigger->SetBoxExtent(TriggerExtent);
	m_trigger->SetCollisionProfileName(TEXT("Trigger"));
	m_trigger->SetCanEverAffectNavigation(false);

	m_promptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Prompt"));
	m_promptWidget->SetupAttachment(m_root);
	m_promptWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
	m_promptWidget->SetWidgetSpace(EWidgetSpace::Screen);   // dreht sich immer zur Kamera
	m_promptWidget->SetDrawSize(FVector2D(220.0f, 60.0f));
	m_promptWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	m_promptWidget->SetVisibility(false);
}

void ADoor::BeginPlay()
{
	Super::BeginPlay();

	{
		TArray<UPrimitiveComponent*> prims;
		GetComponents<UPrimitiveComponent>(prims);
		for (UPrimitiveComponent* p : prims)
			if (p) p->SetCanEverAffectNavigation(false);
	}

	if (m_trigger)
	{
		m_trigger->SetUsingAbsoluteScale(true);
		m_trigger->SetBoxExtent(TriggerExtent);
	}

	if (bAutoOpen)
	{
		m_trigger->OnComponentBeginOverlap.AddDynamic(this, &ADoor::OnTriggerBegin);
		m_trigger->OnComponentEndOverlap.AddDynamic(this, &ADoor::OnTriggerEnd);
	}

	if (m_promptWidget)
	{
		if (PromptWidgetClass)
			m_promptWidget->SetWidgetClass(PromptWidgetClass);
		m_promptWidget->SetRelativeLocation(PromptOffset);
		m_promptWidget->SetVisibility(false);
	}

	m_currentYaw = 0.0f;
	m_hinge->SetRelativeRotation(FRotator::ZeroRotator);
}

void ADoor::ShowPrompt(bool _bShow)
{
	if (m_promptWidget && PromptWidgetClass)
		m_promptWidget->SetVisibility(_bShow);
}

void ADoor::OnInteract(AActor* _interactor)
{
	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(_interactor))
		player->PlayDoorGesture();

	ToggleDoor();
}

void ADoor::Tick(float _dt)
{
	Super::Tick(_dt);

	const float target = m_bOpen ? OpenAngle : 0.0f;
	m_currentYaw = FMath::FInterpTo(m_currentYaw, target, _dt, InterpSpeed);
	m_hinge->SetRelativeRotation(FRotator(0.0f, m_currentYaw, 0.0f));

	if (FMath::IsNearlyEqual(m_currentYaw, target, 0.05f))
	{
		m_currentYaw = target;
		m_hinge->SetRelativeRotation(FRotator(0.0f, m_currentYaw, 0.0f));
		SetActorTickEnabled(false);

		if (m_bOpen)
			ReleaseWaitingNPCs();
	}
}

void ADoor::OpenDoor()
{
	if (m_bOpen) return;
	m_bOpen = true;
	SetActorTickEnabled(true);

	if (m_mesh)
		m_mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (OpenSound)
		if (USoundManager* sm = USoundManager::Get(this))
			sm->PlaySoundAtLocation(OpenSound, GetActorLocation(),
				ESoundCategory::SFX, 1.0f, 1.0f, SoundAttenuation);
}

void ADoor::CloseDoor()
{
	if (!m_bOpen) return;
	m_bOpen = false;
	SetActorTickEnabled(true);

	if (m_mesh)
		m_mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (CloseSound)
		if (USoundManager* sm = USoundManager::Get(this))
			sm->PlaySoundAtLocation(CloseSound, GetActorLocation(),
				ESoundCategory::SFX, 1.0f, 1.0f, SoundAttenuation);
}

void ADoor::ToggleDoor()
{
	if (m_bOpen) CloseDoor();
	else         OpenDoor();
}

// ============================================================================
//  Näherungs-Trigger → Öffnen für jeden Pawn (NPC oder Spieler).
//  Es wird mitgezählt, wie viele Pawns drin sind, damit die Tür erst schließt,
//  wenn wirklich alle wieder draußen sind.
//
//  Ist die Tür geschlossen und bNPCStopAndOpen an, HÄLT ein NPC an, wartet, bis
//  die Tür offen ist, und läuft dann weiter. Der Spieler öffnet immer sofort.
// ============================================================================
void ADoor::OnTriggerBegin(UPrimitiveComponent*, AActor* _other, UPrimitiveComponent* _otherComp,
	int32, bool, const FHitResult&)
{
	APawn* pawn = Cast<APawn>(_other);
	if (!pawn) return;

	if (_otherComp != pawn->GetRootComponent()) return;

	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(pawn))
	{
		if (bPlayerMustPressButton)
		{
			player->AddNearbyInteractable(this);   // meldet die Tür als Interact-Ziel an
			ShowPrompt(true);
			return;                                
		}
	}

	++m_occupants;
	if (m_bOpen) return;   

	// Geschlossen: NPC hält an, öffnet bewusst und läuft weiter, sobald sie offen ist.
	ANPCCharacter* npc = Cast<ANPCCharacter>(pawn);
	if (bNPCStopAndOpen && npc && npc->PauseMovementForDoor())
		m_waitingNPCs.Add(npc);

	OpenDoor();
}

void ADoor::OnTriggerEnd(UPrimitiveComponent*, AActor* _other, UPrimitiveComponent* _otherComp, int32)
{
	APawn* pawn = Cast<APawn>(_other);
	if (!pawn) return;

	if (_otherComp != pawn->GetRootComponent()) return;

	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(pawn))
	{
		if (bPlayerMustPressButton)
		{
			player->RemoveNearbyInteractable(this);
			ShowPrompt(false);
			return;
		}
	}

	m_occupants = FMath::Max(0, m_occupants - 1);
	if (m_occupants == 0)
		CloseDoor();
}

void ADoor::ReleaseWaitingNPCs()
{
	for (const TWeakObjectPtr<ANPCCharacter>& n : m_waitingNPCs)
		if (ANPCCharacter* npc = n.Get())
			npc->ResumeMovementFromDoor();
	m_waitingNPCs.Reset();
}
