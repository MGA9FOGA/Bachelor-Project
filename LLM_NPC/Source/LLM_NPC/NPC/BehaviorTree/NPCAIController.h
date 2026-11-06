#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "NPCAIController.generated.h"

class UBehaviorTree;

UCLASS()
class LLM_NPC_API ANPCAIController : public AAIController
{
    GENERATED_BODY()

public:
    ANPCAIController();
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

    virtual void Tick(float _deltaSeconds) override;

protected:
    virtual void OnPossess(APawn* _pawn) override;

private:
    bool m_bLogicPausedForServers = false;
};