#pragma once

#include "CoreMinimal.h"
#include "LLM_NPC/UI/DraggablePanelWidget.h"
#include "LLM_NPC/Economy/InventorySlot.h"
#include "InventoryWidget.generated.h"

class UItemRowWidget;
class UTextBlock;
class UPanelWidget;
class UButton;

/**
 * UInventoryWidget – Inventar-Anzeige des SPIELERS (Items + Gold).
 */
UCLASS()
class LLM_NPC_API UInventoryWidget : public UDraggablePanelWidget
{
	GENERATED_BODY()

public:
	// Aktualisiert die Anzeige (Gold + Items) aus der Quelle (GetSource).
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Refresh();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UItemRowWidget> RowWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 SlotCount = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	float RefreshInterval = 0.5f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& _geometry, float _deltaTime) override;

	virtual bool GetSource(TArray<FInventorySlot>& _outSlots, int32& _outGold) const;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtGold;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnBack;

private:
	UFUNCTION()
	void OnBackClicked();


	void EnsureSlots();

	// Schreibt Gold und Items in die bestehenden Slots.
	void BuildList(const TArray<FInventorySlot>& _slots, int32 _gold);

	// Die dauerhaft existierenden Slot-Widgets.
	UPROPERTY()
	TArray<TObjectPtr<UItemRowWidget>> m_slots;

	float  m_timer     = 0.0f;
	uint32 m_signature = 0;   
};

/**
 * UNpcInventoryWidget – identisch, zeigt aber das Inventar des aktuell
 * ANVISIERTEN NPCs (GetTargetedNPC)
 */
UCLASS()
class LLM_NPC_API UNpcInventoryWidget : public UInventoryWidget
{
	GENERATED_BODY()

protected:
	virtual bool GetSource(TArray<FInventorySlot>& _outSlots, int32& _outGold) const override;
};
