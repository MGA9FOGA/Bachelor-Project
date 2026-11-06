#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldClock.generated.h"

UCLASS()
class LLM_NPC_API UWorldClock : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ===== Tickable =====
	virtual void Tick(float _deltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UWorldClock, STATGROUP_Tickables);
	}
	virtual bool IsTickable() const override { return true; }

	// ===== Zeit-API =====
	UFUNCTION(BlueprintPure, Category="Time")
	int32 GetHour() const { return m_hour; }

	UFUNCTION(BlueprintPure, Category="Time")
	int32 GetMinute() const { return m_minute; }

	UFUNCTION(BlueprintPure, Category="Time")
	int32 GetDay() const { return m_day; }


	UFUNCTION(BlueprintPure, Category="Time")
	int32 GetWeekday() const { return ((m_day % 7) + 7) % 7; }

	UFUNCTION(BlueprintPure, Category="Time")
	FString GetWeekdayName() const
	{
		static const TCHAR* Names[7] = {
			TEXT("Monday"), TEXT("Tuesday"), TEXT("Wednesday"), TEXT("Thursday"),
			TEXT("Friday"), TEXT("Saturday"), TEXT("Sunday") };
		return Names[GetWeekday()];
	}

	UFUNCTION(BlueprintPure, Category="Time")
	float GetTimeOfDayNormalized() const { return (m_hour + m_minute / 60.0f) / 24.0f; }

	UFUNCTION(BlueprintCallable, Category="Time")
	void SetTime(int32 _hour, int32 _minute = 0);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Time")
	float RealSecondsPerGameHour = 30.0f; //30 = 1 Spieltag = 12 Minuten

private:
	int32 m_hour   = 6;   // Start um 06:00
	int32 m_minute = 0;
	int32 m_day    = 0;
	float m_accumulator = 0.0f;
};