#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LLM_NPC/Economy/InventorySlot.h"
#include "ItemRowWidget.generated.h"

class UImage;
class UTextBlock;
class UButton;
class UPanelSlot;

// Feuert, wenn die Zeile angeklickt wurde (Index = Position in der Liste).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemRowSelected, int32, Index);

/**
 * UItemRowWidget – eine Zeile in einer Item-Liste (Inventar, NPC-Inventar, Shop).
 */
UCLASS()
class LLM_NPC_API UItemRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//_bShowPrice: true = Shop (Preis + Bestand), false = Inventar (Anzahl).
	void SetSlot(const FInventorySlot& _slot, int32 _index, bool _bShowPrice);

	static void StretchInParent(UPanelSlot* _slot);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Row")
	FVector2D IconSize = FVector2D(32.0f, 32.0f);

	void SetEmpty();

	void SetSelected(bool _bSelected);
	bool IsFilled() const { return m_bFilled; }
	int32 GetIndex() const { return m_index; }

	FOnItemRowSelected OnSelected;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ImgIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtInfo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnSelect;

private:
	UFUNCTION()
	void HandleClicked();

	int32 m_index   = INDEX_NONE;
	bool  m_bFilled = false;
};
