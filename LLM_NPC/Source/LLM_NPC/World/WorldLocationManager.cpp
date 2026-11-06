#include "WorldLocationManager.h"

#include "LLM_NPC/NPC/Components/NPC_ECS.h"
#include "LLM_NPC/World/WorldSpot.h"

void UWorldLocationManager::Initialize(FSubsystemCollectionBase& _collection)
{
    Super::Initialize(_collection);
}

bool UWorldLocationManager::GetLocationByTag(EWorldLocationTag _tag,
                                              FWorldLocation& _outLocation) const
{
    if (!LocationData) return false;

    for (const FWorldLocation& loc : LocationData->Locations)
    {
        if (loc.Tag == _tag)
        {
            _outLocation = loc;
            return true;
        }
    }
    return false;
}

bool UWorldLocationManager::GetLocationForDesire(ENPCDesire _desire, ENPCOccupation _occupation,
    FWorldLocation& _outLocation) const
{
    if (!LocationData || LocationData->Locations.IsEmpty()) return false;

    TArray<FWorldLocation> matches;

    if (_desire == ENPCDesire::Work)
    {
        // Arbeitsorte = Orte, die Workstations haben (nicht über Activity)
        for (const FWorldLocation& loc : LocationData->Locations)
            if (loc.Workstations.Num() > 0)
                matches.Add(loc);

        // berufspassenden Ort bevorzugen
        for (const FWorldLocation& loc : matches)
            if (loc.PreferredBy.Contains(_occupation))
            {
                _outLocation = loc;
                return true;
            }
    }
    else if (_desire == ENPCDesire::Sleep)
    {
        // Schlaforte = Orte, die SleepSpots haben
        for (const FWorldLocation& loc : LocationData->Locations)
            if (loc.SleepSpots.Num() > 0)
                matches.Add(loc);
    }
    else
    {
        // Eat → Activity Eat; alles andere (Wander) → Social
        const ELocationActivity wanted =
            (_desire == ENPCDesire::Eat) ? ELocationActivity::Eat : ELocationActivity::Social;
        for (const FWorldLocation& loc : LocationData->Locations)
            if (loc.Activity == wanted)
                matches.Add(loc);
    }

    if (matches.Num() > 0)
    {
        _outLocation = matches[FMath::RandRange(0, matches.Num() - 1)];
        return true;
    }

    return false;
}

// ===== Generischer Kern (Work UND Sleep) =====
bool UWorldLocationManager::ReserveSpot(const TArray<FWorkstation>& _spots, EWorldLocationTag _tag,
    const FString& _prefix, EntityID _entity, const FVector& _basePos,
    FVector& _outWorldPos, FString& _outAnimTag)
{
    for (int32 i = 0; i < _spots.Num(); i++)
    {
        FString key = FString::Printf(TEXT("%s_%s_%d"),
            *_prefix, *WorldLocationTagToString(_tag), i);

        EntityID* occupant = m_occupiedSpots.Find(key);

        // frei, oder schon von DIESEM NPC belegt
        if (!occupant || *occupant == _entity)
        {
            m_occupiedSpots.Add(key, _entity);
            _outWorldPos = _basePos + _spots[i].Offset;
            _outAnimTag  = _spots[i].AnimationTag;

            return true;
        }
    }

    return false;
}

void UWorldLocationManager::ReleaseSpot(const FString& _prefix, EWorldLocationTag _tag, EntityID _entity)
{
    const FString keyPrefix = FString::Printf(TEXT("%s_%s_"),
        *_prefix, *WorldLocationTagToString(_tag));

    TArray<FString> toRemove;
    for (const auto& pair : m_occupiedSpots)
    {
        if (pair.Key.StartsWith(keyPrefix) && pair.Value == _entity)
            toRemove.Add(pair.Key);
    }
    for (const FString& key : toRemove)
    {
        m_occupiedSpots.Remove(key);
    }
}

// ===== Workstations =====
bool UWorldLocationManager::ReserveWorkstation(EWorldLocationTag _tag, EntityID _entity, FVector& _outWorldPos,
    FString& _outAnimTag)
{
    FWorldLocation loc;
    if (!GetLocationByTag(_tag, loc) || loc.Workstations.IsEmpty()) return false;
    return ReserveSpot(loc.Workstations, _tag, TEXT("WORK"), _entity, loc.Position, _outWorldPos, _outAnimTag);
}

void UWorldLocationManager::ReleaseWorkstation(EWorldLocationTag _tag, EntityID _entity)
{
    ReleaseSpot(TEXT("WORK"), _tag, _entity);
}

// ===== Schlafplätze =====
bool UWorldLocationManager::ReserveSleepSpot(EWorldLocationTag _tag, EntityID _entity, FVector& _outWorldPos,
    FString& _outAnimTag)
{
    FWorldLocation loc;
    if (!GetLocationByTag(_tag, loc) || loc.SleepSpots.IsEmpty()) return false;
    return ReserveSpot(loc.SleepSpots, _tag, TEXT("SLEEP"), _entity, loc.Position, _outWorldPos, _outAnimTag);
}

void UWorldLocationManager::ReleaseSleepSpot(EWorldLocationTag _tag, EntityID _entity)
{
    ReleaseSpot(TEXT("SLEEP"), _tag, _entity);
}

// ===== Actor-basierte Spots =====
void UWorldLocationManager::RegisterSpot(AWorldSpot* _spot)
{
    if (_spot && !m_spots.Contains(_spot))
        m_spots.Add(_spot);
}

AWorldSpot* UWorldLocationManager::ReserveSpotActor(EWorldLocationTag _building, ESpotType _type, EntityID _entity)
{

    for (TObjectPtr<AWorldSpot>& s : m_spots)
    {
        AWorldSpot* spot = s.Get();
        if (spot && spot->Building == _building && spot->Type == _type
            && spot->TryReserve(_entity))
        {
            return spot;   // Aufrufer nutzt spot->GetStandTransform() + Montage/AnimTag
        }
    }
    return nullptr;
}

AWorldSpot* UWorldLocationManager::ReserveNearestSpotActor(ESpotType _type, const FVector& _from, EntityID _entity)
{
    AWorldSpot* best     = nullptr;
    float       bestDist = TNumericLimits<float>::Max();

    for (TObjectPtr<AWorldSpot>& s : m_spots)
    {
        AWorldSpot* spot = s.Get();
        if (!spot || spot->Type != _type || !spot->IsFree())
            continue;

        const float d = FVector::DistSquared(spot->GetActorLocation(), _from);
        if (d < bestDist)
        {
            bestDist = d;
            best     = spot;
        }
    }

    if (best && best->TryReserve(_entity))
        return best;
    return nullptr;
}

bool UWorldLocationManager::GetRandomLocation(EWorldLocationTag _excludeTag,
                                              FWorldLocation& _outLocation) const
{
    if (!LocationData || LocationData->Locations.IsEmpty()) return false;

    TArray<FWorldLocation> candidates;
    for (const FWorldLocation& loc : LocationData->Locations)
    {
        if (loc.Tag != _excludeTag)
            candidates.Add(loc);
    }

    if (candidates.IsEmpty()) return false;

    _outLocation = candidates[FMath::RandRange(0, candidates.Num() - 1)];
    return true;
}

bool UWorldLocationManager::GetLocationForNPC(ENPCOccupation _occupation,
                                               float _hunger, float _exhaustion,
                                               FWorldLocation& _outLocation) const
{
    if (!LocationData || LocationData->Locations.IsEmpty()) return false;

    // Needs → Taverne
    if (_hunger >= 0.6f || _exhaustion >= 0.7f)
        if (GetLocationByTag(EWorldLocationTag::Tavern, _outLocation)) return true;

        
    // Bevorzugte Orte für diesen Beruf
    TArray<FWorldLocation> preferred;
    for (const FWorldLocation& loc : LocationData->Locations)
    {
        if (loc.PreferredBy.Contains(_occupation))
            preferred.Add(loc);
    }

    if (preferred.Num() > 0 && FMath::RandRange(0, 100) < 60)
    {
        _outLocation = preferred[FMath::RandRange(0, preferred.Num() - 1)];
        return true;
    }

    // Zufälliger Fallback
    _outLocation = LocationData->Locations[
        FMath::RandRange(0, LocationData->Locations.Num() - 1)];
    return true;
}