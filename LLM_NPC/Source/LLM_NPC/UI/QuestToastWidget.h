#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LLM_NPC/Economy/QuestData.h"
#include "QuestToastWidget.generated.h"

class UTextBlock;

/**
 * UQuestToastWidget – kurze "Neue Quest"-Einblendung, wenn der Spieler eine Quest
 * erhält.
 *   TxtTitle · TxtObjective  (TextBlocks)
 */
UCLASS()
class LLM_NPC_API UQuestToastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests")
	float DisplaySeconds = 3.0f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& _geometry, float _deltaTime) override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Quests")
	void OnQuestToastShown();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtObjective;

private:
	// Abonniert OnQuestAdded des Spielers – einmalig, sobald er verfügbar ist.
	void EnsureBoundToPlayer();

	UFUNCTION()
	void HandleQuestAdded(const FQuestData& _quest);

	void Hide();

	bool m_bBound = false;
	FTimerHandle m_hideTimer;
};
