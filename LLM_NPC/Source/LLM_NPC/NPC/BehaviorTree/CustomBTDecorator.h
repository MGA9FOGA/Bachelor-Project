#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "LLM_NPC/NPC/Components/NPC_ECS.h"
#include "CustomBTDecorator.generated.h"

enum class EIntentType : uint8;

UCLASS()
class LLM_NPC_API UBTDecorator_CheckIntent : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckIntent();

	UPROPERTY(EditAnywhere, Category="Intent")
	EIntentType RequiredIntent = EIntentType::Neutral;

protected:
	virtual bool CalculateRawConditionValue(
		UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};