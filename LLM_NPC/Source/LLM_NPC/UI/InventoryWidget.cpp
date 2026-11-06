#include "InventoryWidget.h"

#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "LLM_NPC/UI/ItemRowWidget.h"
#include "LLM_NPC/LLM_NPCCharacter.h"
#include "LLM_NPC/NPC/NPCCharacter.h"

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BtnBack)
		BtnBack->OnClicked.AddDynamic(this, &UInventoryWidget::OnBackClicked);

	EnsureSlots();
	Refresh();
}

void UInventoryWidget::OnBackClicked()
{
	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn()))
		player->BackToChatView();
}

void UInventoryWidget::NativeTick(const FGeometry& _geometry, float _deltaTime)
{
	Super::NativeTick(_geometry, _deltaTime);

	// Nur aktualisieren, wenn sichtbar – sonst laufen versteckte Panels mit.
	if (RefreshInterval <= 0.0f || GetVisibility() == ESlateVisibility::Collapsed
		|| GetVisibility() == ESlateVisibility::Hidden)
		return;

	m_timer += _deltaTime;
	if (m_timer < RefreshInterval) return;
	m_timer = 0.0f;

	// Daten holen und nur bei echter Änderung neu zeichnen. 
	TArray<FInventorySlot> slots;
	int32 gold = 0;
	GetSource(slots, gold);

	uint32 sig = GetTypeHash(gold);
	for (const FInventorySlot& s : slots)
	{
		sig = HashCombine(sig, GetTypeHash(s.ItemId));
		sig = HashCombine(sig, GetTypeHash(s.Count));
	}

	if (sig != m_signature)
	{
		m_signature = sig;
		BuildList(slots, gold);
	}
}

bool UInventoryWidget::GetSource(TArray<FInventorySlot>& _outSlots, int32& _outGold) const
{
	const ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn());
	if (!player) return false;

	_outSlots = player->GetInventorySlots();
	_outGold  = player->GetGold();
	return true;
}

void UInventoryWidget::Refresh()
{
	TArray<FInventorySlot> slots;
	int32 gold = 0;
	GetSource(slots, gold);
	BuildList(slots, gold);
}

void UInventoryWidget::EnsureSlots()
{
	if (!ItemList || !RowWidgetClass)   return;
	if (m_slots.Num() == SlotCount)     return; 

	ItemList->ClearChildren();
	m_slots.Reset();

	for (int32 i = 0; i < SlotCount; ++i)
	{
		UItemRowWidget* row = CreateWidget<UItemRowWidget>(this, RowWidgetClass);
		if (!row) continue;

		row->SetEmpty();
		UItemRowWidget::StretchInParent(ItemList->AddChild(row));
		m_slots.Add(row);
	}
}

void UInventoryWidget::BuildList(const TArray<FInventorySlot>& _slots, int32 _gold)
{
	if (TxtGold)
		TxtGold->SetText(FText::AsNumber(_gold));

	EnsureSlots();

	for (int32 i = 0; i < m_slots.Num(); ++i)
	{
		UItemRowWidget* row = m_slots[i];
		if (!row) continue;

		if (_slots.IsValidIndex(i))
			row->SetSlot(_slots[i], i, /*bShowPrice*/ false);   // Inventar zeigt die Anzahl
		else
			row->SetEmpty();
	}

}

// ============================================================================
//  NPC-Variante: gleiche Anzeige, andere Quelle
// ============================================================================
bool UNpcInventoryWidget::GetSource(TArray<FInventorySlot>& _outSlots, int32& _outGold) const
{
	const ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn());
	if (!player) return false;

	ANPCCharacter* npc = player->GetTargetedNPC();
	if (!npc) return false;   // niemand anvisiert → leere Liste

	_outSlots = npc->GetInventorySlots();
	_outGold  = npc->GetGold();
	return true;
}
