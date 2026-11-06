#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/IHttpRequest.h"
#include "LLM_NPC/NPC/Components/NPC_ECS.h"
#include "ChatWidget.generated.h"

struct FLLMResponse;
class UScrollBox;
class UEditableTextBox;
class UButton;
class UTextBlock;
class ANPCCharacter;

UCLASS()
class UChatWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SendMessage();
	
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void ClearHistory();
	
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void OpenForNPC(const FString& _npcSystemPrompt, ANPCCharacter* _npc = nullptr);

	ANPCCharacter* GetActiveNPC() const { return m_activeNPC.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Chat")
	void AddNpcMessage(const FString& _text);

	// Boten-Quest-Abgabe: schickt die überbrachte Nachricht als synthetische Spieler-Zeile
	// an den aktiven NPC, damit dieser per LLM darauf antwortet (Antwort landet im Chat).
	// Enthält bewusst kein Trigger-Wort → löst keine neue Quest aus.
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void TriggerDeliveryReply(const FString& _fromNpc, const FString& _message);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UScrollBox* m_messageList;
	
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UEditableTextBox* m_inputTextBox;
	
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* m_sendButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BtnQuests;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BtnInventory;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* TxtNpcName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString m_systemPrompt = TEXT("You are a helpful NPC in a game world. Answer briefly in English.");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	int32 m_maxTokens = 256;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	float m_typewriterSpeed = 0.02f;

	// Eine Quest wird NUR vergeben, wenn die Spieler-Nachricht eines dieser Wörter
	// enthält (English, case-insensitive Substring). Erste Zeile =
	// allgemein (Fetch), zweite Zeile = Boten-/Nachrichten-Quests. WICHTIG: bei Änderung
	// den Wortsatz im Prompt (ANPCCharacter::BuildDialogueContext, QUESTS-Regel) mitziehen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	TArray<FString> QuestTriggerWords = {
		TEXT("work"), TEXT("job"), TEXT("task"), TEXT("quest"), TEXT("help"), TEXT("hire"),
		TEXT("message"), TEXT("deliver"), TEXT("errand"), TEXT("letter"), TEXT("tell"), TEXT("send") };
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LLM")
	EIntentType  m_lastIntent = EIntentType::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LLM")
	EEmotionType m_lastMood   = EEmotionType::Neutral;
	
	// 0.04f = ~25 Zeichen/Sekunde  →  normales Tippen
	// 0.02f = ~50 Zeichen/Sekunde  →  schnell
	// 0.08f = ~12 Zeichen/Sekunde  →  dramatisch langsam

private:
	//Typewriter
	FString          m_pendingTypewriterText;
	FString          m_currentDisplayText;
	int32            m_typewriterIndex = 0;
	FTimerHandle     m_typewriterTimer;
	UTextBlock*      m_currentNPCTextBlock = nullptr;

	void StartTypewriter(const FString& _fullText);
	void TypewriterTick();
	
	struct FChatEntry
	{
		FString Role;
		FString Content;
	};
	
	TArray<FChatEntry> m_conversationHistory;
	FString m_pendingRequestID;

	// Letzte Spieler-Nachricht (für den Quest-Trigger-Wort-Check).
	FString m_lastUserMessage;

	// True, wenn m_lastUserMessage eines der QuestTriggerWords enthält (case-insensitive).
	bool PlayerAskedForWork() const;

	// Kern von SendMessage: setzt einen Dialog-Request mit _userText ab (UI-Zeile "You" +
	// Verlauf + Schema + m_pendingRequestID). SendMessage und TriggerDeliveryReply teilen ihn.
	void SubmitUserTurn(const FString& _userText);

	TWeakObjectPtr<ANPCCharacter> m_activeNPC;

	FString m_npcDisplayName = TEXT("NPC");
	
	FString BuildPrompt(const FString& _userMessage) const;
	FString ExtractNPCResponse(const FString& _rawResponse) const;
	void AddMessageToUI(const FString& _from, const FString& _message, FLinearColor _color = FLinearColor::White);

	// Lädt den (Laufzeit-)Dialogverlauf des aktiven NPCs in UI + m_conversationHistory
	void LoadHistoryFromNPC();
	
	UFUNCTION()
	void OnLLMResponse(const FLLMResponse& _response, bool _bSuccess);
	
	UFUNCTION()
	void OnSendButtonClicked();
	
	UFUNCTION()
	void OnInputCommitted(const FText& _text, ETextCommit::Type _commitMethod);

	UFUNCTION()
	void OnQuestBoardClicked();

	UFUNCTION()
	void OnInventoryClicked();
};
