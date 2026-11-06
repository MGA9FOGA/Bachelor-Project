#include "ChatWidget.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "LLM_NPC/LLM_Server/LLMRequestManager.h"
#include "LLM_NPC/LLM_Server/LLMSchemas.h"
#include "LLM_NPC/NPC/NPCCharacter.h"
#include "LLM_NPC/LLM_NPCCharacter.h"
#include "Dom/JsonObject.h"

void UChatWidget::SendMessage()
{
	if (!m_inputTextBox) return;

	FString userText = m_inputTextBox->GetText().ToString().TrimStartAndEnd();
	if (userText.IsEmpty()) return;

	m_lastUserMessage = userText;   // für den Quest-Trigger-Wort-Check
	m_inputTextBox->SetText(FText::GetEmpty());

	SubmitUserTurn(userText);
}

// Setzt einen Dialog-Request mit _userText ab (gemeinsamer Kern von SendMessage und
// TriggerDeliveryReply). Zeigt die Spieler-Zeile, hängt Verlauf/Schema an und merkt sich
// die RequestID, damit OnLLMResponse nur DIESE Antwort verarbeitet.
void UChatWidget::SubmitUserTurn(const FString& _userText)
{
	ALLMRequestManager* manager = ALLMRequestManager::Get();
	if (!manager)
	{
		AddMessageToUI(TEXT("Error"), TEXT("LLM request manager not available!"), FLinearColor(1.f, 0.4f, 0.4f));
		return;
	}

	AddMessageToUI(TEXT("You"), _userText, FLinearColor(0.5f, 0.85f, 1.0f));

	// Strukturierten Request
	FLLMRequest request;
	request.RequestID   = FGuid::NewGuid().ToString();
	request.MaxTokens   = m_maxTokens;
	request.Timestamp   = 0.0f;
	request.Priority    = 5;
	request.SystemPrompt = m_systemPrompt;
	request.UserMessage  = _userText;
	request.NPCName      = m_npcDisplayName;
	request.JsonSchema   = LLMSchemas::Dialogue();   // erzwingt valides Reply/Intent/Mood(+Quest)

	// Verlauf übertragen – nur ein kleines rollendes Fenster, da die
	const int32 historyWindow = 6;
	const int32 startIdx = FMath::Max(0, m_conversationHistory.Num() - historyWindow);
	for (int32 i = startIdx; i < m_conversationHistory.Num(); i++)
	{
		request.HistoryRoles.Add(m_conversationHistory[i].Role);
		request.HistoryContents.Add(m_conversationHistory[i].Content);
	}


	m_conversationHistory.Add({TEXT("user"), _userText});

	// Spieler-Zeile im NPC persistieren (fuers Wiederanzeigen)
	if (ANPCCharacter* npc = m_activeNPC.Get())
		npc->AddDialogTurn(_userText, false);

	// Nur Antworten mit genau dieser ID akzeptieren (sonst reagiert das
	// Widget auf Antworten anderer NPCs, die am selben Delegate hängen)
	m_pendingRequestID = request.RequestID;
	manager->SubmitRequest(request);

	AddMessageToUI(m_npcDisplayName, TEXT("..."), FLinearColor(0.6f, 0.6f, 0.6f));
}

void UChatWidget::TriggerDeliveryReply(const FString& _fromNpc, const FString& _message)
{
	// Die überbrachte Nachricht als Spieler-Zeile formulieren. Enthält bewusst KEIN
	// Trigger-Wort → PlayerAskedForWork()==false, der antwortende NPC macht daraus keine
	// neue Quest, reagiert aber ganz normal per LLM (Antwort landet im Chat-Verlauf).
	const FString synthetic = FString::Printf(
		TEXT("(I bring you a message from %s: \"%s\")"), *_fromNpc, *_message);
	m_lastUserMessage = synthetic;
	SubmitUserTurn(synthetic);
}

void UChatWidget::ClearHistory()
{
	m_conversationHistory.Empty();
	if (m_messageList)
	{
		m_messageList->ClearChildren();
	}
}

void UChatWidget::OpenForNPC(const FString& _npcSystemPrompt, ANPCCharacter* _npc)
{
	// Prompt setzen
	m_systemPrompt = _npcSystemPrompt;

	// NPC merken, damit die Antwort gezielt an ihn zurückgeht
	m_activeNPC = _npc;
	m_npcDisplayName = _npc ? _npc->GetNPCName() : TEXT("NPC");
	m_pendingRequestID.Empty();

	// Name in die Chat-Kopfzeile schreiben (falls der Textblock im WBP existiert).
	if (TxtNpcName)
		TxtNpcName->SetText(FText::FromString(m_npcDisplayName));

	// Frueheren Verlauf dieses NPCs wieder anzeigen (Laufzeit-Persistenz)
	LoadHistoryFromNPC();

	// Proaktive Begrüßung: hat der NPC (beim Anvisieren über Qwen) schon eine Eröffnungs-
	// zeile vorbereitet, eröffnet er das Gespräch damit. Einmalig – wird beim Abholen geleert.
	if (_npc && _npc->HasPendingGreeting())
		AddNpcMessage(_npc->ConsumePendingGreeting());

	// Widget anzeigen
	SetVisibility(ESlateVisibility::Visible);

	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Chat] Opened for NPC"));
}

void UChatWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (m_sendButton)
	{
		m_sendButton->OnClicked.AddDynamic(this, &UChatWidget::OnSendButtonClicked);
	}
	
	if (m_inputTextBox)
	{
		m_inputTextBox->OnTextCommitted.AddDynamic(this, &UChatWidget::OnInputCommitted);
	}

	if (BtnQuests)
	{
		BtnQuests->OnClicked.AddDynamic(this, &UChatWidget::OnQuestBoardClicked);
	}

	if (BtnInventory)
	{
		BtnInventory->OnClicked.AddDynamic(this, &UChatWidget::OnInventoryClicked);
	}

	//Delegate
	if (ALLMRequestManager* requestManager = ALLMRequestManager::Get())
	{
		requestManager->OnResponseReceived.AddDynamic(this, &UChatWidget::OnLLMResponse);
	}
	else
	{
		AddMessageToUI(TEXT("System"), TEXT("LLM request manager not found! Place the actor in the level."), FLinearColor(1.f, 0.4f, 0.4f));
	}
}

void UChatWidget::NativeDestruct()
{
	if (ALLMRequestManager* requestManager = ALLMRequestManager::Get())
	{
		requestManager->OnResponseReceived.RemoveDynamic(this, &UChatWidget::OnLLMResponse);
	}
	
	Super::NativeDestruct();
}

FString UChatWidget::ExtractNPCResponse(const FString& _rawResponse) const
{
	// Rohe Antwort zeilenweise durchsuchen
	// Erste Zeile die mit "Spieler:", "Du:", "Player:", "User:" beginnt → alles danach wegwerfen
	TArray<FString> Lines;
	_rawResponse.ParseIntoArrayLines(Lines, true);

	FString Result;
	for (const FString& Line : Lines)
	{
		FString TrimmedLine = Line.TrimStart();

		// Erkennt wenn das Modell eine neue Sprecher-Zeile beginnt
		if (TrimmedLine.StartsWith(TEXT("Spieler:"))
		 || TrimmedLine.StartsWith(TEXT("Du:"))
		 || TrimmedLine.StartsWith(TEXT("Player:"))
		 || TrimmedLine.StartsWith(TEXT("User:"))
		 || TrimmedLine.StartsWith(TEXT("NPC:")))   
		{
			break;
		}

		if (!Result.IsEmpty()) Result += TEXT(" ");
		Result += TrimmedLine;
	}

	return Result.TrimStartAndEnd();
}

void UChatWidget::StartTypewriter(const FString& _fullText)
{
	// laufenden Typewriter stoppen falls noch aktiv
	if (GetWorld())
		GetWorld()->GetTimerManager().ClearTimer(m_typewriterTimer);

	m_pendingTypewriterText = _fullText;
	m_currentDisplayText    = TEXT("");
	m_typewriterIndex       = 0;

	// Neues TextBlock für diese Antwort erstellen
	m_currentNPCTextBlock = NewObject<UTextBlock>(this);
	m_currentNPCTextBlock->SetAutoWrapText(true);
	m_currentNPCTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 1.0f, 0.5f)));

	FSlateFontInfo font = m_currentNPCTextBlock->GetFont();
	font.Size = 14;
	m_currentNPCTextBlock->SetFont(font);
	m_currentNPCTextBlock->SetText(
		FText::FromString(FString::Printf(TEXT("[%s]: "), *m_npcDisplayName)));

	m_messageList->AddChild(m_currentNPCTextBlock);
	m_messageList->ScrollToEnd();

	// Timer: alle 40ms ein Zeichen hinzufügen (~25 Zeichen/Sekunde)
	GetWorld()->GetTimerManager().SetTimer(
		m_typewriterTimer,
		this,
		&UChatWidget::TypewriterTick,
		m_typewriterSpeed,
		true  // looping
	);

	// NPC beginnt zu „reden", solange der Typewriter läuft → Antwort-Animation.
	if (ANPCCharacter* npc = m_activeNPC.Get())
		npc->PlayTalkReply();
}

void UChatWidget::TypewriterTick()
{
	if (m_typewriterIndex >= m_pendingTypewriterText.Len())
	{
		GetWorld()->GetTimerManager().ClearTimer(m_typewriterTimer);
		if (ANPCCharacter* npc = m_activeNPC.Get())
			npc->EndTalkReply();
		return;
	}

	// Nächstes Zeichen anhängen
	m_currentDisplayText += m_pendingTypewriterText[m_typewriterIndex];
	m_typewriterIndex++;

	if (m_currentNPCTextBlock)
	{
		m_currentNPCTextBlock->SetText(
			FText::FromString(FString::Printf(TEXT("[%s]: "), *m_npcDisplayName) + m_currentDisplayText));
		m_messageList->ScrollToEnd();
	}
}

FString UChatWidget::BuildPrompt(const FString& _userMessage) const
{
	FString prompt = m_systemPrompt;
	// Explizit einschränken: nur eine einzige NPC-Zeile, kein Spieler-Text
	prompt += TEXT("\nImportant: Answer ONLY as the NPC, in English. Write exactly one reply. Do not start a new 'Player:' line.\n\n");

	for (const FChatEntry& entry : m_conversationHistory)
	{
		if (entry.Role == TEXT("user"))
			prompt += FString::Printf(TEXT("Spieler: %s\n"), *entry.Content);
		else
			prompt += FString::Printf(TEXT("NPC: %s\n"), *entry.Content);
	}

	// Leerzeichen nach "NPC: " damit das Modell direkt weiterschreibt
	prompt += FString::Printf(TEXT("Spieler: %s\nNPC: "), *_userMessage);
	return prompt;
}

void UChatWidget::AddMessageToUI(const FString& _from, const FString& _message, FLinearColor _color)
{
	if (!m_messageList) return;

	UTextBlock* TextBlock = NewObject<UTextBlock>(this);
	TextBlock->SetText(FText::FromString(FString::Printf(TEXT("[%s]: %s"), *_from, *_message)));
	TextBlock->SetAutoWrapText(true);
	TextBlock->SetColorAndOpacity(FSlateColor(_color));

	FSlateFontInfo Font = TextBlock->GetFont();
	Font.Size = 14;
	TextBlock->SetFont(Font);

	m_messageList->AddChild(TextBlock);
	m_messageList->ScrollToEnd();
}

void UChatWidget::LoadHistoryFromNPC()
{
	// UI + Arbeitskopie leeren
	m_conversationHistory.Empty();
	if (m_messageList)
		m_messageList->ClearChildren();

	ANPCCharacter* npc = m_activeNPC.Get();
	if (!npc) return;

	FDialogData* dialog = npc->GetDialog();
	if (!dialog) return;

	UE_LOG(LogTemp, Warning, TEXT("[Chat] Verlauf von NPC '%s' geladen (%d Turns)"),
		*npc->GetNPCName(), dialog->DialogHistory.Num());

	for (const FDialogTurn& turn : dialog->DialogHistory)
	{
		const FString role = turn.bIsNPCResponse ? TEXT("assistant") : TEXT("user");
		m_conversationHistory.Add({ role, turn.DialogText });

		if (turn.bIsNPCResponse)
			AddMessageToUI(npc->GetNPCName(), turn.DialogText, FLinearColor(0.6f, 1.0f, 0.5f));
		else
			AddMessageToUI(TEXT("You"), turn.DialogText, FLinearColor(0.5f, 0.85f, 1.0f));
	}
}

void UChatWidget::OnLLMResponse(const FLLMResponse& _response, bool _bSuccess)
{
	// Nur die Antwort auf UNSERE zuletzt gesendete Chat-Anfrage verarbeiten.
	// Ist keine offen (ID leer) ODER passt die ID nicht → ignorieren.
	if (m_pendingRequestID.IsEmpty() || _response.RequestID != m_pendingRequestID)
	{
		return;
	}
	m_pendingRequestID.Empty();

	UE_LOG(LogTemp, Warning,
		TEXT("[Chat] Antwort akzeptiert fuer NPC '%s' (RequestID=%s, Server=%s)"),
		m_activeNPC.IsValid() ? *m_activeNPC->GetNPCName() : TEXT("(kein NPC)"),
		*_response.RequestID, *_response.ServerUsed);

	if (m_messageList && m_messageList->GetChildrenCount() > 0)
		m_messageList->RemoveChildAt(m_messageList->GetChildrenCount() - 1);

	if (!_bSuccess || _response.Content.IsEmpty() ||
		_response.Content.StartsWith(TEXT("Error")))
	{
		AddMessageToUI(TEXT("Error"), TEXT("No response."), FLinearColor(1.f, 0.4f, 0.4f));
		return;
	}

	// Parsing zentral im „Gehirn" des NPCs (LLM-JSON → Reply/Intent/Mood + Quest).
	FString reply;
	if (ANPCCharacter* npc = m_activeNPC.Get())
	{
		const FNPCDialogResponse parsed = npc->ParseLLMResponse(_response.Content);
		reply        = parsed.Reply;
		m_lastIntent = parsed.Intent;
		m_lastMood   = parsed.Mood;

		if (!parsed.bParseSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("[Chat] JSON PARSE FAILED | raw content: %s"), *_response.Content);
			reply = ExtractNPCResponse(_response.Content);
		}

		// Vom LLM mitgelieferte Quest → NUR annehmen, wenn der Spieler in seiner letzten
		// Nachricht ein Trigger-Wort (work/job/task/...) benutzt hat.
		if (parsed.bHasQuest && PlayerAskedForWork())
		{
			if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn()))
				player->AddQuest(parsed.Quest);
		}
	}
	else
	{
		reply = ExtractNPCResponse(_response.Content);   // (sollte nicht passieren)
	}

	if (reply.IsEmpty())
	{
		AddMessageToUI(TEXT("Error"), TEXT("Empty response."), FLinearColor(1.f, 0.4f, 0.4f));
		return;
	}

	StartTypewriter(reply);
	m_conversationHistory.Add({TEXT("assistant"), reply});
	while (m_conversationHistory.Num() > 20)
		m_conversationHistory.RemoveAt(0);

	// Ergebnis gezielt an den NPC weitergeben (Emotion / Memory / Blackboard + Verlauf).
	if (ANPCCharacter* npc = m_activeNPC.Get())
	{
		npc->AddDialogTurn(reply, true);
		npc->ApplyDialogueResult(reply, m_lastIntent, m_lastMood);
	}
}

void UChatWidget::OnSendButtonClicked()
{
	SendMessage();
}

void UChatWidget::OnQuestBoardClicked()
{
	// Zeigt NUR das NPC-Quest-Fenster (Chat wird versteckt → Abgabe-Buttons klickbar).
	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn()))
		player->ShowNpcQuestBoard();
}

void UChatWidget::OnInventoryClicked()
{
	// Zeigt NUR das NPC-Inventar.
	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(GetOwningPlayerPawn()))
		player->ShowNpcInventoryOnly();
}

void UChatWidget::AddNpcMessage(const FString& _text)
{
	if (_text.IsEmpty()) return;

	AddMessageToUI(m_npcDisplayName, _text);
	m_conversationHistory.Add({ TEXT("assistant"), _text });
	while (m_conversationHistory.Num() > 20)
		m_conversationHistory.RemoveAt(0);
}

bool UChatWidget::PlayerAskedForWork() const
{
	const FString msg = m_lastUserMessage.ToLower();
	for (const FString& w : QuestTriggerWords)
		if (!w.IsEmpty() && msg.Contains(w.ToLower()))
			return true;
	return false;
}

void UChatWidget::OnInputCommitted(const FText& _text, ETextCommit::Type _commitMethod)
{
	if (_commitMethod == ETextCommit::OnEnter)
	{
		SendMessage();
	}
}
