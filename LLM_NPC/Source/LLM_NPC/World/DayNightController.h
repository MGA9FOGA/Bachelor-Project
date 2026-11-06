#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DayNightController.generated.h"

class ADirectionalLight;

/**
 * Passt die "Sonne" (DirectionalLight) an die Uhrzeit der WorldClock an:
 * Helligkeit, Farbe und – optional – den Sonnenstand (Drehung).
 *
 * Einrichtung im Editor:
 *  - Diesen Actor ins Level legen.
 *  - SunLight = die DirectionalLight aus dem Level zuweisen.
 */
UCLASS()
class LLM_NPC_API ADayNightController : public AActor
{
	GENERATED_BODY()

public:
	ADayNightController();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight")
	TObjectPtr<ADirectionalLight> SunLight;

	// ----- Helligkeit -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Brightness")
	float DayIntensity = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Brightness")
	float NightIntensity = 0.1f;

	// ----- Farben -----
	// Mittag (neutralweiß), Horizont (Sonnenauf-/untergang, warm), Nacht (kühl/blau).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Color")
	FLinearColor DayColor = FLinearColor(1.0f, 0.97f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Color")
	FLinearColor HorizonColor = FLinearColor(1.0f, 0.45f, 0.18f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Color")
	FLinearColor NightColor = FLinearColor(0.18f, 0.25f, 0.5f);

	// ----- Sonnenstand -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Rotation")
	bool bDriveSunRotation = true;

	// Himmelsrichtung (Yaw) der Sonnenbahn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Rotation")
	float SunYaw = -30.0f;

	// ----- Mond -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Moon")
	TObjectPtr<ADirectionalLight> MoonLight;

	// Maximale Mond-Helligkeit (mitten in der Nacht).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Moon")
	float MoonIntensity = 0.4f;

	// Bläulich-türkises Mondlicht.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DayNight|Moon")
	FLinearColor MoonColor = FLinearColor(0.2f, 0.6f, 0.85f);

private:
	void UpdateSun(float _hourOfDay);
	void UpdateMoon(float _hourOfDay, float _elevation);
};
