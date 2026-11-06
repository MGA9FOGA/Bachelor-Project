#include "NPCAnimInstance.h"
#include "LLM_NPC/NPC/NPCCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UNPCAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	m_owner = Cast<ANPCCharacter>(TryGetPawnOwner());
}

void UNPCAnimInstance::NativeUpdateAnimation(float _deltaSeconds)
{
	Super::NativeUpdateAnimation(_deltaSeconds);

	if (!m_owner)
	{
		m_owner = Cast<ANPCCharacter>(TryGetPawnOwner());
		if (!m_owner)
			return;
	}

	GroundSpeed = m_owner->GetVelocity().Size2D();
	bIsMoving   = GroundSpeed > MoveSpeedThreshold;

	if (const UCharacterMovementComponent* move = m_owner->GetCharacterMovement())
		bIsInAir = move->IsFalling();
}
