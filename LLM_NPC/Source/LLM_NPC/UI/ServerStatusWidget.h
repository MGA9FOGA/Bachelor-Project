#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ServerStatusWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * Zeigt zwei Status-LEDs (Kreise) fuer Hermes und Qwen.
 * Gruen = online, Rot = offline.
 *
 * Pingt selbst live den /health-Endpunkt der beiden Server 
 * aktuellen Zustand: wird gruen sobald der Server bereit ist, rot wenn er weg ist.
 *
 */
UCLASS()
class LLM_NPC_API UServerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* ImgHermes = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* ImgQwen = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* TxtHermes = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* TxtQwen = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Status")
	FLinearColor OnlineColor = FLinearColor(0.15f, 0.85f, 0.25f, 1.0f);   // gruen

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Status")
	FLinearColor OfflineColor = FLinearColor(0.85f, 0.15f, 0.15f, 1.0f);  // rot

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Status")
	float UpdateInterval = 1.0f;

public:
	UFUNCTION(BlueprintPure, Category = "Server Status")
	bool IsHermesOnline() const { return m_bHermesOnline; }

	UFUNCTION(BlueprintPure, Category = "Server Status")
	bool IsQwenOnline() const { return m_bQwenOnline; }

private:
	// Pingt beide Server (liest Ports aus den Managern, Fallback 8080/8081)
	void PollHealth();
	// Async /health-Check fuer einen Port; setzt das Flag und faerbt neu
	void CheckServerHealth(int32 _port, bool _bHermes);
	
	void ApplyColors();

	bool m_bHermesOnline = false;
	bool m_bQwenOnline   = false;

	FTimerHandle m_pollTimer;
};
