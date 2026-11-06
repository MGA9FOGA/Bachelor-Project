#include "QuestLogWidget.h"

#include "QuestRowWidget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBoxSlot.h"
#include "LLM_NPC/LLM_NPCCharacter.h"

void UQuestLogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureBoundToPlayer();
	Refresh();
}

void UQuestLogWidget::NativeTick(const FGeometry& _geometry, float _deltaTime)
{
	Super::NativeTick(_geometry, _deltaTime);

	if (!m_bBound)
		EnsureBoundToPlayer();
}

void UQuestLogWidget::NativeDestruct()
{
	if (m_bBound)
		if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn()))
			player->OnQuestsChanged.RemoveDynamic(this, &UQuestLogWidget::Refresh);
	m_bBound = false;
	Super::NativeDestruct();
}

void UQuestLogWidget::EnsureBoundToPlayer()
{
	if (m_bBound) return;

	ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn());
	if (!player) return;

	player->OnQuestsChanged.AddDynamic(this, &UQuestLogWidget::Refresh);
	m_bBound = true;
	Refresh();  
}

void UQuestLogWidget::Refresh()
{
	if (!QuestList) return;

	QuestList->ClearChildren();

	ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn());
	const TArray<FQuestData> quests = player ? player->GetActiveQuests() : TArray<FQuestData>();

	if (RowWidgetClass)
	{
		for (const FQuestData& q : quests)
		{
			UQuestRowWidget* row = CreateWidget<UQuestRowWidget>(this, RowWidgetClass);
			if (!row) continue;
			row->SetQuest(q);

			UPanelSlot* slot = QuestList->AddChild(row);
			if (UVerticalBoxSlot* vb = Cast<UVerticalBoxSlot>(slot))
			{
				vb->SetHorizontalAlignment(HAlign_Fill);
				vb->SetPadding(FMargin(0.0f, 4.0f));
			}
			else if (UScrollBoxSlot* sb = Cast<UScrollBoxSlot>(slot))
			{
				sb->SetHorizontalAlignment(HAlign_Fill);
				sb->SetPadding(FMargin(0.0f, 4.0f));
			}
		}
	}

	if (TxtEmpty)
		TxtEmpty->SetVisibility(quests.Num() == 0 ? ESlateVisibility::Visible
		                                          : ESlateVisibility::Collapsed);
}
