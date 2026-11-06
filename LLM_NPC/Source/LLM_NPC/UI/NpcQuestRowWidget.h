#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LLM_NPC/Economy/QuestData.h"
#include "NpcQuestRowWidget.generated.h"

class UTextBlock;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestTurnInClicked, const FString&, QuestId);

/**
 * UNpcQuestRowWidget – eine Zeile im NPC-Quest-Board: zeigt eine Quest DIESES NPCs und
 *   TxtTitle · TxtObjective · TxtReward · BtnTurnIn (Button)
 */
UCLASS()
class LLM_NPC_API UNpcQuestRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetQuest(const FQuestData& _quest);

	FOnQuestTurnInClicked OnTurnInClicked;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtObjective;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtReward;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnTurnIn;

private:
	UFUNCTION()
	void HandleClicked();

	FString m_questId;
};
