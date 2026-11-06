#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"


UINTERFACE(MinimalAPI, Blueprintable)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class LLM_NPC_API IInteractableInterface
{
	GENERATED_BODY()

public:
	virtual void OnInteract(AActor* _interactor) = 0;

	virtual FString GetInteractionPrompt() = 0;

	virtual bool CanInteract() const = 0;

	// true  → Interaktion öffnet das Chat-Fenster (NPCs).
	// false → Interaktion wird direkt in OnInteract behandelt (z.B. Marktstand-Shop),
	//         ohne den Chat zu öffnen.
	virtual bool UsesChatUI() const { return true; }
};