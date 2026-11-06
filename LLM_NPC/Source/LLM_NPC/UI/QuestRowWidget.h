#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LLM_NPC/Economy/QuestData.h"
#include "QuestRowWidget.generated.h"

class UTextBlock;
class UButton;
class UWidget;

UCLASS()
class LLM_NPC_API UQuestRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetQuest(const FQuestData& _quest);

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void SetExpanded(bool _bExpanded);

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void ToggleExpanded();

	bool IsExpanded() const { return m_bExpanded; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests")
	bool bStartExpanded = false;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Quests")
	void OnExpandedChanged(bool bExpanded);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnHeader;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DetailsBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtObjective;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtReward;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtGiver;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtDescription;

private:
	UFUNCTION()
	void HandleHeaderClicked();

	bool m_bExpanded = false;
};
