#pragma once

#include "CoreMinimal.h"
#include "LLM_NPC/UI/DraggablePanelWidget.h"
#include "LLM_NPC/Economy/InventorySlot.h"
#include "ShopWidget.generated.h"

class AMarketStall;
class ALLM_NPCCharacter;
class UItemRowWidget;
class UTextBlock;
class UButton;
class UPanelWidget;

/**
 * UShopWidget – Kauf-UI eines Marktstands (von AMarketStall::OnInteract geöffnet).
 *
 *   TxtStallName, TxtGold (TextBlock) · OfferList (ScrollBox/VerticalBox)
 *   BtnBuy, BtnClose (Button)
 */
UCLASS()
class LLM_NPC_API UShopWidget : public UDraggablePanelWidget
{
	GENERATED_BODY()

public:

	void Setup(AMarketStall* _stall, ALLM_NPCCharacter* _buyer);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void Refresh();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void CloseShop();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TSubclassOf<UItemRowWidget> RowWidgetClass;

protected:
	virtual void NativeConstruct() override;
	// Sicherheitsnetz: hebt die Spieler-Sperre auf, falls das Widget ohne CloseShop verschwindet.
	virtual void NativeDestruct() override;
	// Schließt den Shop automatisch, sobald der Stand zumacht (Öffnungszeit vorbei).
	virtual void NativeTick(const FGeometry& _geometry, float _deltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtStallName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtGold;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> OfferList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnBuy;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnClose;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnTalk;

private:
	UFUNCTION() void HandleBuy();
	UFUNCTION() void HandleClose();
	UFUNCTION() void HandleTalk();
	UFUNCTION() void HandleRowSelected(int32 _index);

	void UpdateSelectionVisuals();

	TWeakObjectPtr<AMarketStall>      m_stall;
	TWeakObjectPtr<ALLM_NPCCharacter> m_buyer;

	UPROPERTY()
	TArray<TObjectPtr<UItemRowWidget>> m_rows;

	int32 m_selected = INDEX_NONE;
};
