#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldStateProvider.generated.h"

/**
 * UWorldStateProvider – die orthogonale Weltzustands-Schicht (Kap. 8 der Architektur).
 *
 * Liefert Kontext, den KEIN einzelner NPC aus seinen eigenen Komponenten ableiten
 * kann, in drei Ebenen:
 *   1. Zeit      – Uhrzeit, Wochentag, Tageszeit-Phase (aus UWorldClock)
 *   2. Orte      – bekannte Orte des Dorfes (aus UWorldLocationManager)
 *   3. Wirtschaft – welche Marktstände gerade geöffnet sind (AMarketStall)
 *
 * Der FPromptBuilder stellt diesen Block automatisch VOR den NPC-spezifischen
 * Kontext jedes Hermes-Prompts. So bleibt das LLM entkoppelt: es sieht nur Text.
 */
UCLASS()
class LLM_NPC_API UWorldStateProvider : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Bequemer Zugriff von jedem UObject mit Welt-Kontext aus.
	static UWorldStateProvider* Get(const UObject* _worldContext);

	// Ebene 1: Uhrzeit + Wochentag + Tageszeit.
	UFUNCTION(BlueprintPure, Category="WorldState")
	FString GetTimeState() const;

	// Ebene 2: bekannte Orte im Dorf.
	UFUNCTION(BlueprintPure, Category="WorldState")
	FString GetPlacesState() const;

	// Ebene 3: aktuell geöffnete Marktstände.
	UFUNCTION(BlueprintPure, Category="WorldState")
	FString GetEconomyState() const;

	// Kompletter Weltzustands-Block als Prompt-Präfix ("WORLD STATE:\n- …").
	UFUNCTION(BlueprintPure, Category="WorldState")
	FString BuildBlock() const;

private:
	UWorld* GetWorldSafe() const;
};
