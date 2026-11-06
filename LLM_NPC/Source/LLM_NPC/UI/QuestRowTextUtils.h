#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"

// Gemeinsame Anzeige-Helfer für die Quest-Zeilen (Log- UND Board-Row).
namespace QuestRowText
{
	// Kürzt einen Titel für die Anzeige: höchstens _maxChars Zeichen, sonst mit „…".
	inline FString Abbrev(const FString& _s, int32 _maxChars)
	{
		const FString t = _s.TrimStartAndEnd();
		if (t.Len() <= _maxChars) return t;
		return t.Left(FMath::Max(1, _maxChars - 1)).TrimEnd() + TEXT("…");
	}

	// Text bleibt einzeilig und wird an der eigenen (durch die Border begrenzten) Breite
	// mit „…" abgeschnitten, statt umzubrechen oder über die Border hinauszuwachsen.
	inline void ClipOneLine(UTextBlock* _t)
	{
		if (!_t) return;
		_t->SetAutoWrapText(false);
		_t->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		_t->SetClipping(EWidgetClipping::ClipToBounds);
	}
}
