#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LLM_NPC/NPC/Components/NPCWorldEnums.h"
#include "NPCAnimInstance.generated.h"

class ANPCCharacter;

/**
 * ============================================================================
 *  UNPCAnimInstance – Basis-AnimInstance für alle NPCs
 * ============================================================================
 *
 * Macht die Per-Frame-Logik für die LOCOMOTION in C++ und stellt dem Animation-
 * Blueprint saubere Variablen bereit:
 *
 *   - GroundSpeed : horizontale Geschwindigkeit (für den 1D-Locomotion-Blendspace)
 *   - bIsMoving   : läuft der NPC gerade?
 *   - bIsInAir    : fällt/springt er gerade? (für später)
 *
 * Die TÄTIGKEITS-Animationen (sitzen/essen/arbeiten) laufen NICHT hierüber,
 * sondern als Montagen am jeweiligen AWorldSpot (Intro→Loop), die über den
 * Slot 'DefaultSlot' des AnimGraphs gespielt werden. Das AnimBP braucht also
 * nur den Blendspace + DefaultSlot, keine Posen-State-Machine.
 *
 */
UCLASS()
class LLM_NPC_API UNPCAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float _deltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim")
	bool bIsInAir = false;

	UPROPERTY(EditDefaultsOnly, Category = "Anim")
	float MoveSpeedThreshold = 3.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ANPCCharacter> m_owner;
};
