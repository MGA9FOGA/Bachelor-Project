#include "ItemRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/PanelSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBoxSlot.h"

void UItemRowWidget::StretchInParent(UPanelSlot* _slot)
{
	if (UVerticalBoxSlot* vb = Cast<UVerticalBoxSlot>(_slot))
	{
		vb->SetHorizontalAlignment(HAlign_Fill);
		vb->SetPadding(FMargin(0.0f, 2.0f));
	}
	else if (UScrollBoxSlot* sb = Cast<UScrollBoxSlot>(_slot))
	{
		sb->SetHorizontalAlignment(HAlign_Fill);
		sb->SetPadding(FMargin(0.0f, 2.0f));
	}
}

void UItemRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BtnSelect)
		BtnSelect->OnClicked.AddDynamic(this, &UItemRowWidget::HandleClicked);


	if (ImgIcon)
		ImgIcon->SetDesiredSizeOverride(IconSize);

}

void UItemRowWidget::SetEmpty()
{
	m_bFilled = false;
	m_index   = INDEX_NONE;

	if (TxtName) TxtName->SetText(FText::GetEmpty());
	if (TxtInfo) TxtInfo->SetText(FText::GetEmpty());
	if (ImgIcon) ImgIcon->SetVisibility(ESlateVisibility::Hidden);

	SetSelected(false);
}

void UItemRowWidget::SetSlot(const FInventorySlot& _slot, int32 _index, bool _bShowPrice)
{
	m_index   = _index;
	m_bFilled = true;

	if (TxtName)
		TxtName->SetText(FText::FromString(_slot.DisplayName));

	if (TxtInfo)
	{
		FString info;
		if (_bShowPrice)
		{
			// Shop: Preis + (falls begrenzt) Bestand.
			info = FString::Printf(TEXT("%d G"), _slot.Price);
			if (_slot.Count >= 0)
				info += FString::Printf(TEXT("  (%d)"), _slot.Count);
		}
		else
		{
			// Inventar: nur die Anzahl.
			info = FString::Printf(TEXT("x%d"), _slot.Count);
		}
		TxtInfo->SetText(FText::FromString(info));
	}

	if (ImgIcon)
	{
		if (_slot.Icon)
		{
			// bMatchSize = false: die Texturgröße soll gerade NICHT durchschlagen.
			ImgIcon->SetBrushFromTexture(_slot.Icon, /*bMatchSize*/ false);
			ImgIcon->SetDesiredSizeOverride(IconSize);
			ImgIcon->SetVisibility(ESlateVisibility::Visible);
		}
	}

	SetSelected(false);
}

void UItemRowWidget::SetSelected(bool _bSelected)
{
	if (BtnSelect)
		BtnSelect->SetBackgroundColor(_bSelected ? FLinearColor(0.9f, 0.75f, 0.2f, 1.0f)
		                                         : FLinearColor::White);
}

void UItemRowWidget::HandleClicked()
{
	if (!m_bFilled) return;   // leere Slots sind nicht auswählbar
	OnSelected.Broadcast(m_index);
}
