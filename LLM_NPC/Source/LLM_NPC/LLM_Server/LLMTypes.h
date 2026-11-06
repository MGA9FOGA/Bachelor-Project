#pragma once

#include "CoreMinimal.h"

// ============================================================
//  Routing-Entscheidung: welcher llama.cpp-Server wird genutzt
// ============================================================
enum class ELLMRoute : uint8
{
	Hermes,		// Port 8080 – Llama-3, hohe Qualität, Primärmodell
	Qwen		// Port 8081 – Qwen3, /no_think, Fallback + NPC-NPC
};

// ============================================================
//  Anfrage-Priorität (höher = früher in der Hermes-PrioQueue)
// ============================================================
enum class ELLMPriority : uint8
{
	Low    = 1,   // Hintergrundaufgaben, Nacht-Planung
	Normal = 5,   // Standard-NPC-Dialogue
	High   = 10,  // Spieler spricht direkt mit NPC
	Urgent = 20   // Needs
};

// ============================================================
//  Zentraler Request-Container
//  Wird vom Game Thread erstellt, vom Worker Thread konsumiert
// ============================================================
//struct FLLMRequest
//{
//	FString NPCId;        // ECS Entity-ID des NPC
//	FString Prompt;       // Formatierter Prompt-String
//	int32   Priority = static_cast<int32>(ELLMPriority::Normal);
//
//	// Callback läuft garantiert auf dem Game Thread (via AsyncTask)
//	TFunction<void(const FString& Response)> Callback;
//
//	// Für std::priority_queue: höherer Wert = früher verarbeitet
//	bool operator<(const FLLMRequest& Other) const
//	{
//		return Priority < Other.Priority;
//	}
//};