#include "WorldClock.h"
#include "LLM_NPC/LLM_Server/LLMRequestManager.h"


void UWorldClock::Tick(float _deltaTime)
{

	if (!ALLMRequestManager::AreServersReady(GetWorld()))
		return;

	m_accumulator += _deltaTime;

	float gameMinutesPerRealSecond = 60.0f / RealSecondsPerGameHour;
	float minutesToAdd = m_accumulator * gameMinutesPerRealSecond;

	if (minutesToAdd >= 1.0f)
	{
		int32 wholeMinutes = FMath::FloorToInt(minutesToAdd);
		m_accumulator -= wholeMinutes / gameMinutesPerRealSecond;

		m_minute += wholeMinutes;
		while (m_minute >= 60)
		{
			m_minute -= 60;
			const int32 prevHour = m_hour;
			m_hour = (m_hour + 1) % 24;
			if (m_hour == 0 && prevHour == 23)
				++m_day;   
		}
	}
}

void UWorldClock::SetTime(int32 _hour, int32 _minute)
{
	m_hour   = FMath::Clamp(_hour, 0, 23);
	m_minute = FMath::Clamp(_minute, 0, 59);
	m_accumulator = 0.0f;
}
