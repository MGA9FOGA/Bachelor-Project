#include "NpcQuestBoardWidget.h"

#include "NpcQuestRowWidget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "LLM_NPC/LLM_NPCCharacter.h"
#include "LLM_NPC/NPC/NPCCharacter.h"

void UNpcQuestBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!QuestList)
		UE_LOG(LogTemp, Warning, TEXT("[NpcQuestBoard] Kein Container 'QuestList' gebunden."));
	if (!RowWidgetClass)
		UE_LOG(LogTemp, Warning, TEXT("[NpcQuestBoard] RowWidgetClass ist nicht gesetzt."));

	if (TxtResponse)
		TxtResponse->SetVisibility(ESlateVisibility::Collapsed);

	if (BtnBack)
		BtnBack->OnClicked.AddDynamic(this, &UNpcQuestBoardWidget::OnBackClicked);

	EnsureBoundToPlayer();
	Refresh();
}

void UNpcQuestBoardWidget::NativeTick(const FGeometry& _geometry, float _deltaTime)
{
	Super::NativeTick(_geometry, _deltaTime);
	if (!m_bBound)
		EnsureBoundToPlayer();
}

void UNpcQuestBoardWidget::NativeDestruct()
{
	if (m_bBound)
		if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn()))
			player->OnQuestsChanged.RemoveDynamic(this, &UNpcQuestBoardWidget::Refresh);
	m_bBound = false;
	Super::NativeDestruct();
}

void UNpcQuestBoardWidget::EnsureBoundToPlayer()
{
	if (m_bBound) return;
	ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn());
	if (!player) return;

	player->OnQuestsChanged.AddDynamic(this, &UNpcQuestBoardWidget::Refresh);
	m_bBound = true;
}

void UNpcQuestBoardWidget::Refresh()
{
	if (!QuestList) return;

	QuestList->ClearChildren();

	ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn());
	ANPCCharacter*     npc    = player ? player->GetTargetedNPC() : nullptr;
	const FString      npcName = npc ? npc->GetNPCName() : FString();

	if (TxtNpcName)
		TxtNpcName->SetText(FText::FromString(npcName));

	int32 shown = 0;
	if (player && npc && RowWidgetClass)
	{
		for (const FQuestData& q : player->GetActiveQuests())
		{
			// Fetch-Quests werden beim GEBER abgegeben, Boten-Quests beim ZIEL-NPC.
			const bool bGiverHere   = q.Type != EQuestType::Delivery && q.GiverName == npcName;
			const bool bDeliverHere = q.Type == EQuestType::Delivery
				&& q.TargetNpc.Equals(npcName, ESearchCase::IgnoreCase);
			if (!bGiverHere && !bDeliverHere) continue;

			UNpcQuestRowWidget* row = CreateWidget<UNpcQuestRowWidget>(this, RowWidgetClass);
			if (!row) continue;
			row->SetQuest(q);
			row->OnTurnInClicked.AddDynamic(this, &UNpcQuestBoardWidget::HandleTurnIn);
			QuestList->AddChild(row);
			++shown;
		}
	}

	if (TxtEmpty)
		TxtEmpty->SetVisibility(shown == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UNpcQuestBoardWidget::HandleTurnIn(const FString& _questId)
{
	ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn());
	if (!player) return;

	FQuestData q;
	bool found = false;
	for (const FQuestData& a : player->GetActiveQuests())
		if (a.QuestId == _questId) { q = a; found = true; break; }
	if (!found) return;

	// Boten-Quest: Abgabe beim Ziel-NPC. Der merkt sich die Nachricht, reflektiert und
	// antwortet per LLM im Chat – deshalb hier KEINE feste Antwort, sondern zum Chat wechseln.
	if (q.Type == EQuestType::Delivery)
	{
		player->DeliverMessageQuest(_questId);
		return;
	}

	const bool ok = player->TryCompleteQuest(_questId);

	// Feste Antwort des NPCs.
	const FString reply = ok
		? FString::Printf(TEXT("Thank you for %d x %s! Here is your reward: %d gold."),
		                  q.TargetCount, *q.TargetItem.ToString(), q.RewardGold)
		: FString::Printf(TEXT("You are still missing something - I need %d x %s."),
		                  q.TargetCount, *q.TargetItem.ToString());

	if (ok)
		player->PostNpcChatLine(reply);
	if (TxtResponse)
	{
		TxtResponse->SetText(FText::FromString(reply));
		TxtResponse->SetVisibility(ESlateVisibility::Visible);
	}

	// Bei Erfolg feuert TryCompleteQuest OnQuestsChanged → Refresh() rebaut die Liste.
	if (!ok)
		Refresh();
}

void UNpcQuestBoardWidget::OnBackClicked()
{
	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn()))
		player->BackToChatView();
}
