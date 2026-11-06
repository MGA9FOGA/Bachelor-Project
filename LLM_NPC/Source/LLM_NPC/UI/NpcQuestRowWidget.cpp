#include "NpcQuestRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "QuestRowTextUtils.h"   

void UNpcQuestRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BtnTurnIn)
		BtnTurnIn->OnClicked.AddDynamic(this, &UNpcQuestRowWidget::HandleClicked);

	QuestRowText::ClipOneLine(TxtTitle);
	QuestRowText::ClipOneLine(TxtObjective);
	QuestRowText::ClipOneLine(TxtReward);
}

void UNpcQuestRowWidget::SetQuest(const FQuestData& _quest)
{
	m_questId = _quest.QuestId;

	if (TxtTitle)
	{
		const FString title = _quest.Title.IsEmpty() ? _quest.QuestId : _quest.Title;
		TxtTitle->SetText(FText::FromString(QuestRowText::Abbrev(title, 30)));
	}

	if (TxtObjective)
	{
		FString obj;
		if (_quest.Type == EQuestType::Delivery)
			obj = FString::Printf(TEXT("Message from %s: %s"),
				*_quest.GiverName, *_quest.Description);
		else if (!_quest.TargetItem.IsNone() && _quest.TargetCount > 0)
			obj = FString::Printf(TEXT("Bring %d x %s"), _quest.TargetCount, *_quest.TargetItem.ToString());
		else
			obj = _quest.Description;
		TxtObjective->SetText(FText::FromString(obj));
	}

	if (TxtReward)
		TxtReward->SetText(FText::FromString(FString::Printf(TEXT("%d Gold"), _quest.RewardGold)));
}

void UNpcQuestRowWidget::HandleClicked()
{
	if (!m_questId.IsEmpty())
		OnTurnInClicked.Broadcast(m_questId);
}
