#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClockWidget.generated.h"

class UTextBlock;

/**
 * Zeigt die Spielzeit (HH:MM) der UWorldClock an.
 */
UCLASS()
class LLM_NPC_API UClockWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* TxtTime = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* TxtWeekday = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock")
	float UpdateInterval = 0.25f;

public:
	UFUNCTION(BlueprintPure, Category = "Clock")
	FString GetTimeString() const;

	UFUNCTION(BlueprintPure, Category = "Clock")
	FString GetWeekdayString() const;

private:
	void UpdateTime();

	FTimerHandle m_timer;
};
