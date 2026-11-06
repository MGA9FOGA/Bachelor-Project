#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LLM_NPC/Interface/InteractableInterface.h"
#include "Door.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UUserWidget;
class ANPCCharacter;
class USoundBase;
class USoundAttenuation;

/**
 * ADoor – eine schwenkbare Tür, die um ein Scharnier dreht.
 *
 * Öffnet/schließt automatisch, sobald ein Pawn (NPC ODER Spieler) in die
 * Trigger-Box kommt bzw. sie verlässt (bAutoOpen). Zusätzlich lässt sie sich
 * über das IInteractableInterface bewusst schalten (OnInteract → Toggle),
 * z.B. per Spieler-Interaktion oder NPC-Trace.
 */
UCLASS()
class LLM_NPC_API ADoor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ADoor();

	// === Konfiguration ===
	UPROPERTY(EditAnywhere, Category="Door", meta=(ClampMin=-180, ClampMax=180))
	float OpenAngle = 90.0f;

	// Schwenkgeschwindigkeit 
	UPROPERTY(EditAnywhere, Category="Door", meta=(ClampMin=0.5))
	float InterpSpeed = 4.0f;

	// Sounds beim Öffnen/Schließen laufen über den USoundManager
	UPROPERTY(EditAnywhere, Category="Door|Sound")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, Category="Door|Sound")
	TObjectPtr<USoundBase> CloseSound;

	UPROPERTY(EditAnywhere, Category="Door|Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	// true → öffnet automatisch bei Pawn in der Trigger-Box; false → nur per OnInteract/OpenDoor.
	UPROPERTY(EditAnywhere, Category="Door")
	bool bAutoOpen = true;

	UPROPERTY(EditAnywhere, Category="Door", meta=(ClampMin=20))
	FVector TriggerExtent = FVector(110.0f, 110.0f, 110.0f);

	// true  → der NPC läuft zur Tür, HÄLT AN, öffnet sie (falls noch zu), wartet bis sie
	//         offen ist und geht ERST DANN weiter zum Ziel (bewusster, „handelnder" Agent).
	//         Ist die Tür schon offen, hält er nicht an. (Standard)
	// false → NPCs öffnen die Tür im Vorbeigehen (Trigger-Box) und laufen ohne Halt weiter.
	UPROPERTY(EditAnywhere, Category="Door")
	bool bNPCStopAndOpen = true;

	// true  → der SPIELER muss die Interact-Taste drücken (Prompt erscheint über der
	//         Tür, solange er in Reichweite ist).
	// false → der Spieler öffnet automatisch im Vorbeigehen (wie ein NPC).
	UPROPERTY(EditAnywhere, Category="Door")
	bool bPlayerMustPressButton = true;

	// Widget, das über der Tür eingeblendet wird, solange der Spieler in Reichweite ist
	UPROPERTY(EditAnywhere, Category="Door|UI")
	TSubclassOf<UUserWidget> PromptWidgetClass;

	// Position des Prompts relativ zum Tür-Ursprung:
	// X = vor/zurück, Y = links/rechts, Z = Höhe.
	UPROPERTY(EditAnywhere, Category="Door|UI")
	FVector PromptOffset = FVector(0.0f, 0.0f, 220.0f);

	// === Steuerung ===
	UFUNCTION(BlueprintCallable, Category="Door")
	void OpenDoor();

	UFUNCTION(BlueprintCallable, Category="Door")
	void CloseDoor();

	UFUNCTION(BlueprintCallable, Category="Door")
	void ToggleDoor();

	UFUNCTION(BlueprintPure, Category="Door")
	bool IsOpen() const { return m_bOpen; }

	// === Interactable (für bewusstes Öffnen: Spieler-Taste oder NPC-Trace) ===
	virtual void OnInteract(AActor* _interactor) override;
	virtual FString GetInteractionPrompt() override { return m_bOpen ? TEXT("Close door") : TEXT("Open door"); }
	virtual bool CanInteract() const override { return true; }
	virtual bool UsesChatUI() const override { return false; }   // öffnet Tür, keinen Chat

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float _dt) override;

private:
	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<USceneComponent> m_root;

	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<USceneComponent> m_hinge;

	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<UStaticMeshComponent> m_mesh;

	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<UBoxComponent> m_trigger;

	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<UWidgetComponent> m_promptWidget;

	void ShowPrompt(bool _bShow);

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* _comp, AActor* _other, UPrimitiveComponent* _otherComp,
		int32 _bodyIndex, bool _bFromSweep, const FHitResult& _sweep);
	UFUNCTION()
	void OnTriggerEnd(UPrimitiveComponent* _comp, AActor* _other, UPrimitiveComponent* _otherComp,
		int32 _bodyIndex);

	bool  m_bOpen = false;       // Zielzustand (offen/zu)
	float m_currentYaw = 0.0f;   // aktueller Schwenkwinkel (wird interpoliert)
	int32 m_occupants = 0;       // Anzahl Pawns aktuell in der Trigger-Box

	UPROPERTY()
	TArray<TWeakObjectPtr<ANPCCharacter>> m_waitingNPCs;

	void ReleaseWaitingNPCs();
};
