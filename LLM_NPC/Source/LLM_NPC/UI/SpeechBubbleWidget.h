#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SpeechBubbleWidget.generated.h"

class UTextBlock;

/**
 * USpeechBubbleWidget – kleine Sprechblase über einem NPC. 
 */
UCLASS()
class LLM_NPC_API USpeechBubbleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowLine(const FString& _text);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TxtLine;
};
