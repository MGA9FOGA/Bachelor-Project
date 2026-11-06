#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SoundManager.generated.h"

class USoundBase;
class USoundAttenuation;


UENUM(BlueprintType)
enum class ESoundCategory : uint8
{
	SFX     UMETA(DisplayName = "SFX"),       // Welt-Effekte (Türen, Markt, Schritte …)
	UI      UMETA(DisplayName = "UI"),        // Menü-/Chat-Klicks, Benachrichtigungen
	Ambient UMETA(DisplayName = "Ambient")    // Hintergrund/Atmo
};

/**
 * USoundManager – zentrale Anlaufstelle für alle Sounds.
 *
 * - 3D-Sounds an einer Weltposition (Türen, Markt …) und 2D-Sounds (UI).
 * - Lautstärke pro Kategorie + Master (0..1), z.B. aus einem Options-Menü.
 *
 * Zugriff von überall:
 *     if (USoundManager* sm = USoundManager::Get(this))
 *         sm->PlaySoundAtLocation(MySound, GetActorLocation());
 */
UCLASS()
class LLM_NPC_API USoundManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& _collection) override;


	static USoundManager* Get(const UObject* _worldContext);


	UFUNCTION(BlueprintCallable, Category = "Sound")
	void PlaySoundAtLocation(USoundBase* _sound, const FVector& _location,
		ESoundCategory _category = ESoundCategory::SFX, float _volume = 1.0f, float _pitch = 1.0f,
		USoundAttenuation* _attenuation = nullptr);

	// 2D-Sound ohne Position (UI-Klicks, Hinweise).
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void PlaySound2D(USoundBase* _sound, ESoundCategory _category = ESoundCategory::UI,
		float _volume = 1.0f, float _pitch = 1.0f);

	// === Lautstärke-Steuerung (0..1) ===
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void SetCategoryVolume(ESoundCategory _category, float _volume);

	UFUNCTION(BlueprintPure, Category = "Sound")
	float GetCategoryVolume(ESoundCategory _category) const;

	UFUNCTION(BlueprintCallable, Category = "Sound")
	void SetMasterVolume(float _volume) { m_masterVolume = FMath::Clamp(_volume, 0.0f, 1.0f); }

	UFUNCTION(BlueprintPure, Category = "Sound")
	float GetMasterVolume() const { return m_masterVolume; }

private:
	// Endlautstärke = Eingabe × Kategorie × Master.
	float EffectiveVolume(ESoundCategory _category, float _volume) const;

	// Aktuelle Welt des GameInstance (für UGameplayStatics-Aufrufe).
	UWorld* GetWorldSafe() const;

	float m_masterVolume = 1.0f;
	TMap<ESoundCategory, float> m_categoryVolumes;
};
