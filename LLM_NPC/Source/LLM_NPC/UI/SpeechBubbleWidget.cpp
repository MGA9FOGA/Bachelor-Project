#include "SpeechBubbleWidget.h"
#include "Components/TextBlock.h"

void USpeechBubbleWidget::ShowLine(const FString& _text)
{
	if (!TxtLine)
	{
		return;
	}
	TxtLine->SetText(FText::FromString(_text));
}
