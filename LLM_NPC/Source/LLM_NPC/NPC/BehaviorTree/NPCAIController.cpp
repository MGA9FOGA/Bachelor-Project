#include "NPCAIController.h"

#include "NPCBehaviorTree.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "LLM_NPC/NPC/NPCCharacter.h"
#include "LLM_NPC/LLM_Server/LLMRequestManager.h"

ANPCAIController::ANPCAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANPCAIController::Tick(float _deltaSeconds)
{
	Super::Tick(_deltaSeconds);

	UBrainComponent* brain = GetBrainComponent();
	if (!brain) return;

	const bool bReady = ALLMRequestManager::AreServersReady(GetWorld());

	if (!bReady && !m_bLogicPausedForServers)
	{
		brain->PauseLogic(TEXT("Warte auf LLM-Server"));
		m_bLogicPausedForServers = true;
	}
	else if (bReady && m_bLogicPausedForServers)
	{
		brain->ResumeLogic(TEXT("LLM-Server bereit"));
		m_bLogicPausedForServers = false;
	}
}

void ANPCAIController::OnPossess(APawn* _pawn)
{
	Super::OnPossess(_pawn);

	if (!BehaviorTreeAsset)
	{
		if (ANPCCharacter* npc = Cast<ANPCCharacter>(_pawn))
			BehaviorTreeAsset = npc->GetBehaviorTree();
	}

	if (!BehaviorTreeAsset)
	{
		return;
	}

	// Blackboard aus dem BT-Asset initialisieren
	if (BehaviorTreeAsset->BlackboardAsset)
	{
		UBlackboardComponent* BB = nullptr;
		UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BB);

		if (BB)
		{
			BB->SetValueAsBool(FNPCBlackboardKeys::IsHungry,    false);
			BB->SetValueAsBool(FNPCBlackboardKeys::IsExhausted, false);
		}
	}

	RunBehaviorTree(BehaviorTreeAsset);

	// Sind die Server noch nicht da → BT sofort pausieren 
	if (!ALLMRequestManager::AreServersReady(GetWorld()))
	{
		if (UBrainComponent* brain = GetBrainComponent())
		{
			brain->PauseLogic(TEXT("Warte auf LLM-Server"));
			m_bLogicPausedForServers = true;
		}
	}
}