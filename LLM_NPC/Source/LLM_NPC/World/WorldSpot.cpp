#include "WorldSpot.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/GameInstance.h"
#include "LLM_NPC/World/WorldLocationManager.h"

AWorldSpot::AWorldSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	m_root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(m_root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(m_root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	NPCStandSlot = CreateDefaultSubobject<USceneComponent>(TEXT("NPCStandSlot"));
	NPCStandSlot->SetupAttachment(m_root);
	StandSlots.Add(NPCStandSlot);

	for (int32 i = 1; i < MaxSlots; ++i)
	{
		USceneComponent* slot =
			CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("StandSlot_%d"), i));
		slot->SetupAttachment(m_root);
		StandSlots.Add(slot);
	}

	PlayerSlot = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerSlot"));
	PlayerSlot->SetupAttachment(m_root);

#if WITH_EDITORONLY_DATA
	
	for (int32 i = 0; i < StandSlots.Num(); ++i)
	{
		UArrowComponent* arrow =
			CreateDefaultSubobject<UArrowComponent>(*FString::Printf(TEXT("FacingArrow_%d"), i));
		arrow->SetupAttachment(StandSlots[i]);
		m_facingArrows.Add(arrow);
	}
#endif
}

void AWorldSpot::BeginPlay()
{
	Super::BeginPlay();

	m_occupants.Init(0, UsableSlots());

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorldLocationManager* mgr = GI->GetSubsystem<UWorldLocationManager>())
		{
			mgr->RegisterSpot(this);
		}
	}
}

int32 AWorldSpot::UsableSlots() const
{
	return FMath::Clamp(Capacity, 1, StandSlots.Num());
}

FTransform AWorldSpot::GetStandTransformFor(uint32 _entity) const
{
	for (int32 i = 0; i < m_occupants.Num(); ++i)
	{
		if (m_occupants[i] == _entity && StandSlots.IsValidIndex(i) && StandSlots[i])
			return StandSlots[i]->GetComponentTransform();
	}

	if (StandSlots.Num() > 0 && StandSlots[0])
		return StandSlots[0]->GetComponentTransform();
	return GetActorTransform();
}

bool AWorldSpot::IsFree() const
{
	const int32 cap = UsableSlots();
	for (int32 i = 0; i < cap; ++i)
	{
		if (!m_occupants.IsValidIndex(i) || m_occupants[i] == 0)
			return true;
	}
	return false;
}

bool AWorldSpot::TryReserve(uint32 _entity)
{
	const int32 cap = UsableSlots();
	if (m_occupants.Num() < cap)
		m_occupants.SetNumZeroed(cap);

	for (int32 i = 0; i < cap; ++i)
		if (m_occupants[i] == _entity) return true;

	for (int32 i = 0; i < cap; ++i)
	{
		if (m_occupants[i] == 0)
		{
			m_occupants[i] = _entity;
			return true;
		}
	}

	return false;   
}

void AWorldSpot::Release(uint32 _entity)
{
	for (int32 i = 0; i < m_occupants.Num(); ++i)
		if (m_occupants[i] == _entity) m_occupants[i] = 0;
}
