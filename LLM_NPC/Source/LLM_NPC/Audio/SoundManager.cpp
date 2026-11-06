#include "SoundManager.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/GameInstance.h"

void USoundManager::Initialize(FSubsystemCollectionBase& _collection)
{
	Super::Initialize(_collection);

	// Start-Lautstärken je Kategorie (voll).
	m_categoryVolumes.Add(ESoundCategory::SFX, 1.0f);
	m_categoryVolumes.Add(ESoundCategory::UI, 1.0f);
	m_categoryVolumes.Add(ESoundCategory::Ambient, 1.0f);
}

USoundManager* USoundManager::Get(const UObject* _worldContext)
{
	if (UGameInstance* gi = UGameplayStatics::GetGameInstance(_worldContext))
		return gi->GetSubsystem<USoundManager>();
	return nullptr;
}

UWorld* USoundManager::GetWorldSafe() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

float USoundManager::EffectiveVolume(ESoundCategory _category, float _volume) const
{
	const float* cat = m_categoryVolumes.Find(_category);
	const float catVol = cat ? *cat : 1.0f;
	return FMath::Clamp(_volume, 0.0f, 4.0f) * catVol * m_masterVolume;
}

void USoundManager::PlaySoundAtLocation(USoundBase* _sound, const FVector& _location,
	ESoundCategory _category, float _volume, float _pitch, USoundAttenuation* _attenuation)
{
	if (!_sound) return;

	const float vol = EffectiveVolume(_category, _volume);
	if (vol <= 0.0f) return;

	if (UWorld* world = GetWorldSafe())
		UGameplayStatics::PlaySoundAtLocation(world, _sound, _location, vol, _pitch,
			/*StartTime*/ 0.0f, _attenuation);
}

void USoundManager::PlaySound2D(USoundBase* _sound, ESoundCategory _category, float _volume, float _pitch)
{
	if (!_sound) return;

	const float vol = EffectiveVolume(_category, _volume);
	if (vol <= 0.0f) return;

	if (UWorld* world = GetWorldSafe())
		UGameplayStatics::PlaySound2D(world, _sound, vol, _pitch);
}

void USoundManager::SetCategoryVolume(ESoundCategory _category, float _volume)
{
	m_categoryVolumes.Add(_category, FMath::Clamp(_volume, 0.0f, 1.0f));
}

float USoundManager::GetCategoryVolume(ESoundCategory _category) const
{
	const float* cat = m_categoryVolumes.Find(_category);
	return cat ? *cat : 1.0f;
}
