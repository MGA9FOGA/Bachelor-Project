#include "QuestToastWidget.h"

#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "LLM_NPC/LLM_NPCCharacter.h"

void UQuestToastWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	EnsureBoundToPlayer();
}

void UQuestToastWidget::NativeTick(const FGeometry& _geometry, float _deltaTime)
{
	Super::NativeTick(_geometry, _deltaTime);

	if (!m_bBound)
		EnsureBoundToPlayer();
}

void UQuestToastWidget::NativeDestruct()
{
	if (UWorld* w = GetWorld())
		w->GetTimerManager().ClearTimer(m_hideTimer);

	if (m_bBound)
		if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn()))
			player->OnQuestAdded.RemoveDynamic(this, &UQuestToastWidget::HandleQuestAdded);

	m_bBound = false;
	Super::NativeDestruct();
}

void UQuestToastWidget::EnsureBoundToPlayer()
{
	if (m_bBound) return;

	ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn());
	if (!player) return;

	player->OnQuestAdded.AddDynamic(this, &UQuestToastWidget::HandleQuestAdded);
	m_bBound = true;
}

void UQuestToastWidget::HandleQuestAdded(const FQuestData& _quest)
{
	if (TxtTitle)
		TxtTitle->SetText(FText::FromString(_quest.Title.IsEmpty() ? TEXT("New quest") : _quest.Title));

	if (TxtObjective)
	{
		FString obj = _quest.Description;
		if (!_quest.TargetItem.IsNone() && _quest.TargetCount > 0)
			obj = FString::Printf(TEXT("Bring %d x %s"), _quest.TargetCount, *_quest.TargetItem.ToString());
		TxtObjective->SetText(FText::FromString(obj));
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	OnQuestToastShown();  

	if (UWorld* w = GetWorld())
	{
		w->GetTimerManager().ClearTimer(m_hideTimer);
		w->GetTimerManager().SetTimer(m_hideTimer, this, &UQuestToastWidget::Hide, DisplaySeconds, false);
	}
}

void UQuestToastWidget::Hide()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
