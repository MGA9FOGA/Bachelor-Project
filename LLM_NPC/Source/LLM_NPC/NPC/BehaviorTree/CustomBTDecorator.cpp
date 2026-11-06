#include "CustomBTDecorator.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTDecorator_CheckIntent::UBTDecorator_CheckIntent()
{
	NodeName = TEXT("Check Intent");
}

bool UBTDecorator_CheckIntent::CalculateRawConditionValue(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return false;

	FString currentIntent = BB->GetValueAsString(TEXT("LastIntent"));
	return currentIntent == IntentToString(RequiredIntent);
}