#pragma once

#include "CoreMinimal.h"

class ANPCCharacter;

/**
 * FPromptBuilder – die Prompt-Aufbau-Schicht (Kap. 8 der Architektur).
 *
 * Aufbau eines Prompts:  [WORLD STATE]  +  [NPC-Kontext]  (+ Regeln/JSON-Schema)
 */
class LLM_NPC_API FPromptBuilder
{
public:
	// Dialog-Prompt (Hermes): Weltzustand + NPC-Kontext + Antwort-Regeln/JSON.
	static FString BuildDialoguePrompt(ANPCCharacter& _npc);

	// Tagesplan-Prompt: Weltzustand + Planungs-Kontext.
	static FString BuildSchedulePrompt(ANPCCharacter& _npc);
};
