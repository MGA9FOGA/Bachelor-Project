#include "ClockWidget.h"

#include "Components/TextBlock.h"
#include "LLM_NPC/World/WorldClock.h"

void UClockWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UpdateTime();

	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimer(
			m_timer, this, &UClockWidget::UpdateTime, UpdateInterval, /*loop*/ true);
	}
}

void UClockWidget::NativeDestruct()
{
	if (UWorld* world = GetWorld())
		world->GetTimerManager().ClearTimer(m_timer);

	Super::NativeDestruct();
}

FString UClockWidget::GetTimeString() const
{
	if (UWorld* world = GetWorld())
	{
		if (UWorldClock* clock = world->GetSubsystem<UWorldClock>())
			return FString::Printf(TEXT("%02d:%02d"), clock->GetHour(), clock->GetMinute());
	}
	return TEXT("--:--");
}

FString UClockWidget::GetWeekdayString() const
{
	static const TCHAR* Names[7] = {
		TEXT("Monday"), TEXT("Tuesday"), TEXT("Wednesday"), TEXT("Thursday"),
		TEXT("Friday"), TEXT("Saturday"), TEXT("Sunday") };
	if (UWorld* world = GetWorld())
		if (UWorldClock* clock = world->GetSubsystem<UWorldClock>())
			return Names[clock->GetWeekday()];
	return TEXT("—");
}

void UClockWidget::UpdateTime()
{
	if (TxtTime)
		TxtTime->SetText(FText::FromString(GetTimeString()));
	if (TxtWeekday)
		TxtWeekday->SetText(FText::FromString(GetWeekdayString()));
}
