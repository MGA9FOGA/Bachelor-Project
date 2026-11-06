#include "ShopWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"
#include "LLM_NPC/UI/ItemRowWidget.h"
#include "LLM_NPC/Economy/MarketStall.h"
#include "LLM_NPC/LLM_NPCCharacter.h"

void UShopWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BtnBuy)   BtnBuy->OnClicked.AddDynamic(this, &UShopWidget::HandleBuy);
	if (BtnClose) BtnClose->OnClicked.AddDynamic(this, &UShopWidget::HandleClose);
	if (BtnTalk)  BtnTalk->OnClicked.AddDynamic(this, &UShopWidget::HandleTalk);


	if (ALLM_NPCCharacter* buyer = m_buyer.Get())
		buyer->SetShopOpen(true);


	Refresh();
}

void UShopWidget::NativeDestruct()
{
	if (ALLM_NPCCharacter* buyer = m_buyer.Get())
		buyer->SetShopOpen(false);
	Super::NativeDestruct();
}

void UShopWidget::NativeTick(const FGeometry& _geometry, float _deltaTime)
{
	Super::NativeTick(_geometry, _deltaTime);

	const AMarketStall* stall = m_stall.Get();
	if (!stall || !stall->CanInteract())
		CloseShop();
}

void UShopWidget::Setup(AMarketStall* _stall, ALLM_NPCCharacter* _buyer)
{
	m_stall = _stall;
	m_buyer = _buyer;
	Refresh();   
}

void UShopWidget::Refresh()
{
	AMarketStall*      stall = m_stall.Get();
	ALLM_NPCCharacter* buyer = m_buyer.Get();

	if (TxtStallName)
		TxtStallName->SetText(FText::FromString(stall ? stall->StallName : FString()));

	if (TxtGold)
		TxtGold->SetText(FText::AsNumber(buyer ? buyer->GetGold() : 0));

	if (!OfferList) return;

	// Liste neu aufbauen.
	OfferList->ClearChildren();
	m_rows.Reset();

	if (!stall || !RowWidgetClass) return;

	const TArray<FInventorySlot> offers = stall->GetOfferSlots();
	for (int32 i = 0; i < offers.Num(); ++i)
	{
		// Leere Einträge (Ware ohne ItemDataAsset) überspringen – der Index bleibt
		if (offers[i].DisplayName.IsEmpty()) continue;

		UItemRowWidget* row = CreateWidget<UItemRowWidget>(this, RowWidgetClass);
		if (!row) continue;

		row->SetSlot(offers[i], i, /*bShowPrice*/ true);
		row->OnSelected.AddDynamic(this, &UShopWidget::HandleRowSelected);
		UItemRowWidget::StretchInParent(OfferList->AddChild(row));
		m_rows.Add(row);
	}

	UpdateSelectionVisuals();
}

void UShopWidget::HandleRowSelected(int32 _index)
{
	m_selected = _index;
	UpdateSelectionVisuals();
}

void UShopWidget::UpdateSelectionVisuals()
{
	for (UItemRowWidget* row : m_rows)
		if (row)
			row->SetSelected(row->GetIndex() == m_selected);
}

void UShopWidget::HandleBuy()
{
	AMarketStall*      stall = m_stall.Get();
	ALLM_NPCCharacter* buyer = m_buyer.Get();
	if (!stall || !buyer || m_selected == INDEX_NONE) return;

	if (stall->TryBuy(buyer, m_selected))
		Refresh();   // Gold + Bestand aktualisieren
}

void UShopWidget::HandleClose()
{
	CloseShop();
}

void UShopWidget::HandleTalk()
{
	AMarketStall*      stall = m_stall.Get();
	ALLM_NPCCharacter* buyer = m_buyer.Get();
	ANPCCharacter*     vendor = stall ? stall->Vendor.Get() : nullptr;
	if (!buyer || !vendor) return;

	CloseShop();
	buyer->StartChatWith(vendor);
}

void UShopWidget::CloseShop()
{

	if (ALLM_NPCCharacter* buyer = m_buyer.Get())
		buyer->SetShopOpen(false);


	if (APlayerController* pc = GetOwningPlayer())
	{
		pc->SetInputMode(FInputModeGameOnly());
		pc->bShowMouseCursor = false;
	}
	RemoveFromParent();
}
