#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NpcQuestBoardWidget.generated.h"

class UNpcQuestRowWidget;
class UPanelWidget;
class UTextBlock;
class UButton;

/**
 * UNpcQuestBoardWidget – Panel beim Reden mit einem NPC: zeigt dessen offene Quests für
 * den Spieler, jede mit Abgabe-Button; nach der Abgabe zeigt es die Antwort des NPCs.
 * Das WBP erbt hiervon und braucht:
 *   QuestList (ScrollBox/VerticalBox) · optional TxtNpcName · TxtResponse · TxtEmpty
 * Zusätzlich `RowWidgetClass` auf dein Zeilen-WBP (erbt von UNpcQuestRowWidget) setzen.
 */
UCLASS()
class LLM_NPC_API UNpcQuestBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Baut die Liste für den aktuell angesprochenen NPC neu auf.
	UFUNCTION(BlueprintCallable, Category = "Quests")
	void Refresh();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests")
	TSubclassOf<UNpcQuestRowWidget> RowWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& _geometry, float _deltaTime) override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> QuestList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtNpcName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtResponse;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtEmpty;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnBack;

private:
	void EnsureBoundToPlayer();

	UFUNCTION()
	void HandleTurnIn(const FString& _questId);

	UFUNCTION()
	void OnBackClicked();

	bool m_bBound = false;
};
