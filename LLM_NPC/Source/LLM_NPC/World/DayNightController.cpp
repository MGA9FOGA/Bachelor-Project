#include "DayNightController.h"

#include "WorldClock.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"

ADayNightController::ADayNightController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADayNightController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* world = GetWorld();
	if (!world || !SunLight) return;

	UWorldClock* clock = world->GetSubsystem<UWorldClock>();
	if (!clock) return;

	// Kontinuierliche Tageszeit (inkl. Minuten) → weiche Übergänge.
	const float hourOfDay = clock->GetTimeOfDayNormalized() * 24.0f;

	// Sonnenhöhe: +1 = Mittag , 0 = Horizont (6/18 Uhr),
	// -1 = Mitternacht.
	const float elevation = FMath::Sin((hourOfDay - 6.0f) / 12.0f * PI);

	UpdateSun(hourOfDay);
	UpdateMoon(hourOfDay, elevation);
}

void ADayNightController::UpdateSun(float _hourOfDay)
{
	// Sonnenhöhe: +1 = Mittag , 0 = Horizont,
	// -1 = Mitternacht ).
	const float elevation = FMath::Sin((_hourOfDay - 6.0f) / 12.0f * PI);

	// ----- Helligkeit -----
	const float dayFactor = FMath::Clamp((elevation + 0.15f) / 0.6f, 0.0f, 1.0f);
	const float intensity = FMath::Lerp(NightIntensity, DayIntensity, dayFactor);

	// ----- Farbe -----
	FLinearColor color;
	if (elevation <= 0.0f)
	{
		const float t = FMath::Clamp((elevation + 0.3f) / 0.3f, 0.0f, 1.0f);
		color = FMath::Lerp(NightColor, HorizonColor, t);
	}
	else
	{
		const float t = FMath::Clamp(elevation / 0.4f, 0.0f, 1.0f);
		color = FMath::Lerp(HorizonColor, DayColor, t);
	}

	if (ULightComponent* lc = SunLight->GetLightComponent())
	{
		lc->SetIntensity(intensity);
		lc->SetLightColor(color);
	}

	// ----- Sonnenstand -----
	if (bDriveSunRotation)
	{
		// 6 Uhr = 0° (Horizont/Aufgang), 12 Uhr = -90° (von oben),
		// 18 Uhr = -180° (Horizont/Untergang), Nacht weiter herum.
		const float pitch = -((_hourOfDay - 6.0f) / 24.0f) * 360.0f;
		SunLight->SetActorRotation(FRotator(pitch, SunYaw, 0.0f));
	}
}

void ADayNightController::UpdateMoon(float _hourOfDay, float _elevation)
{
	if (!MoonLight) return;

	const float moonFactor = FMath::Clamp((-_elevation + 0.15f) / 0.6f, 0.0f, 1.0f);

	if (ULightComponent* lc = MoonLight->GetLightComponent())
	{
		lc->SetIntensity(MoonIntensity * moonFactor);
		lc->SetLightColor(MoonColor);

		lc->SetVisibility(moonFactor > 0.01f);
	}

	// Mondbahn = Sonnenbahn um 12 Stunden versetzt (Mond oben, wenn Sonne unten).
	if (bDriveSunRotation)
	{
		const float pitch = -(((_hourOfDay + 12.0f) - 6.0f) / 24.0f) * 360.0f;
		MoonLight->SetActorRotation(FRotator(pitch, SunYaw, 0.0f));
	}
}
