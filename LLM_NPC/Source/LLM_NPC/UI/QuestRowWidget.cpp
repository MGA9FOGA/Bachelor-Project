#include "QuestRowWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "QuestRowTextUtils.h"   

void UQuestRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BtnHeader)
		BtnHeader->OnClicked.AddDynamic(this, &UQuestRowWidget::HandleHeaderClicked);


	QuestRowText::ClipOneLine(TxtTitle);
	QuestRowText::ClipOneLine(TxtObjective);
	QuestRowText::ClipOneLine(TxtReward);
	QuestRowText::ClipOneLine(TxtGiver);

	SetExpanded(bStartExpanded);
}

void UQuestRowWidget::HandleHeaderClicked()
{
	ToggleExpanded();
}

void UQuestRowWidget::ToggleExpanded()
{
	SetExpanded(!m_bExpanded);
}

void UQuestRowWidget::SetExpanded(bool _bExpanded)
{
	m_bExpanded = _bExpanded;

	if (DetailsBox)
		DetailsBox->SetVisibility(m_bExpanded ? ESlateVisibility::Visible
		                                      : ESlateVisibility::Collapsed);

	OnExpandedChanged(m_bExpanded);   
}

void UQuestRowWidget::SetQuest(const FQuestData& _quest)
{
	if (TxtTitle)
	{
		const FString title = _quest.Title.IsEmpty() ? _quest.QuestId : _quest.Title;
		TxtTitle->SetText(FText::FromString(QuestRowText::Abbrev(title, 30)));
	}

	if (TxtObjective)
	{
		FString obj;
		if (_quest.Type == EQuestType::Delivery)
			obj = FString::Printf(TEXT("Deliver to %s"), *_quest.TargetNpc);
		else if (!_quest.TargetItem.IsNone() && _quest.TargetCount > 0)
			obj = FString::Printf(TEXT("Bring %d x %s"), _quest.TargetCount, *_quest.TargetItem.ToString());
		else
			obj = _quest.Description;
		TxtObjective->SetText(FText::FromString(obj));
	}

	if (TxtReward)
		TxtReward->SetText(FText::FromString(FString::Printf(TEXT("%d Gold"), _quest.RewardGold)));

	if (TxtGiver)
		TxtGiver->SetText(FText::FromString(_quest.GiverName));

	if (TxtDescription)
		TxtDescription->SetText(FText::FromString(_quest.Description));
}
