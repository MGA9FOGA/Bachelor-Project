#include "PromptBuilder.h"

#include "NPCCharacter.h"
#include "LLM_NPC/World/WorldStateProvider.h"

FString FPromptBuilder::BuildDialoguePrompt(ANPCCharacter& _npc)
{
	FString prompt;

	// 1) Weltzustand voranstellen (Zeit / Orte / Wirtschaft).
	if (UWorldStateProvider* ws = UWorldStateProvider::Get(&_npc))
		prompt += ws->BuildBlock();

	// 2) NPC-spezifischer Kontext + Regeln/JSON-Schema.
	prompt += _npc.BuildDialogueContext();
	return prompt;
}

FString FPromptBuilder::BuildSchedulePrompt(ANPCCharacter& _npc)
{
	FString prompt;

	if (UWorldStateProvider* ws = UWorldStateProvider::Get(&_npc))
		prompt += ws->BuildBlock();

	prompt += _npc.BuildScheduleContext();
	return prompt;
}
