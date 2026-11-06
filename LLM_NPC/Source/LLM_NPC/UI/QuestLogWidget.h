#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestLogWidget.generated.h"

class UQuestRowWidget;
class UPanelWidget;
class UTextBlock;

/**
 * UQuestLogWidget – zeigt die aktiven Quests des Spielers. Das WBP erbt hiervon und
 * braucht:
 *   QuestList (ScrollBox/VerticalBox) – Container für die Zeilen
 *   TxtEmpty  (TextBlock, optional)   – Hinweis "keine Quests"
 * Zusätzlich `RowWidgetClass` auf dein Zeilen-WBP (erbt von UQuestRowWidget) setzen.
 * Aktualisiert sich selbst über OnQuestsChanged des Spielers (kein Polling).
 */
UCLASS()
class LLM_NPC_API UQuestLogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Quests")
	void Refresh();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests")
	TSubclassOf<UQuestRowWidget> RowWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& _geometry, float _deltaTime) override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> QuestList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtEmpty;

private:
	void EnsureBoundToPlayer();

	bool m_bBound = false;
};
