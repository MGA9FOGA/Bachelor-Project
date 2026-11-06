# include "NPCCharacter.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/TextRenderComponent.h"     // schwebendes Namens-Label
#include "Kismet/GameplayStatics.h"             // Kamera für die Ausrichtung
#include "Camera/PlayerCameraManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "LLM_NPC/UI/SpeechBubbleWidget.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "NPCDataAsset.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/NPCBehaviorTree.h"
#include "BehaviorTree/NPCAIController.h"
#include "PromptBuilder.h"
#include "Components/NPCBrainComponent.h"
#include "LLM_NPC/LLM_NPCCharacter.h"
#include "LLM_NPC/LLM_NPCGameMode.h"
#include "LLM_NPC/LLM_Server/LLMRequestManager.h"
#include "LLM_NPC/World/WorldClock.h"
#include "LLM_NPC/World/WorldLocationDataAsset.h"
#include "LLM_NPC/World/WorldLocationManager.h"
#include "LLM_NPC/World/WorldSpot.h"
#include "LLM_NPC/Economy/MarketStall.h"
#include "LLM_NPC/Economy/ItemDataAsset.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

ANPCCharacter::ANPCCharacter() : m_ecs()
{
	PrimaryActorTick.TickInterval = 0.016f; //60 FPS

	AIControllerClass = ANPCAIController::StaticClass();
	AutoPossessAI     = EAutoPossessAI::PlacedInWorldOrSpawned;


	if (UCharacterMovementComponent* move = GetCharacterMovement())
		move->MaxWalkSpeed = 165.0f;

	m_interactionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	m_interactionSphere->SetupAttachment(RootComponent);
	m_interactionSphere->SetSphereRadius(200.0f);
	m_interactionSphere->SetCollisionProfileName(TEXT("Trigger"));

	m_awarenessSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AwarenessSphere"));
	m_awarenessSphere->SetupAttachment(RootComponent);
	m_awarenessSphere->SetSphereRadius(GreetingRange);
	m_awarenessSphere->SetCollisionProfileName(TEXT("Trigger"));


	m_brain = CreateDefaultSubobject<UNPCBrainComponent>(TEXT("Brain"));

	m_speechBubble = CreateDefaultSubobject<UWidgetComponent>(TEXT("SpeechBubble"));
	m_speechBubble->SetupAttachment(RootComponent);
	m_speechBubble->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	m_speechBubble->SetWidgetSpace(EWidgetSpace::Screen);
	m_speechBubble->SetDrawSize(FVector2D(240.0f, 64.0f));
	m_speechBubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	m_speechBubble->SetVisibility(false);


	m_nameTag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameTag"));
	m_nameTag->SetupAttachment(RootComponent);
	m_nameTag->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	m_nameTag->SetHorizontalAlignment(EHTA_Center);
	m_nameTag->SetVerticalAlignment(EVRTA_TextBottom);
	m_nameTag->SetWorldSize(22.0f);
	m_nameTag->SetTextRenderColor(FColor::White);
	m_nameTag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	m_nameTag->SetVisibility(false);
}

void ANPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyModularAppearance();   

	if (m_speechBubble && SpeechBubbleWidgetClass)
		m_speechBubble->SetWidgetClass(SpeechBubbleWidgetClass);
	if (m_speechBubble)
		m_speechBubble->SetVisibility(false);

	m_interactionSphere->OnComponentBeginOverlap.AddDynamic(
		this, &ANPCCharacter::OnInteractionSphereBeginOverlap);
	m_interactionSphere->OnComponentEndOverlap.AddDynamic(
		this, &ANPCCharacter::OnInteractionSphereEndOverlap);

	if (m_awarenessSphere)
	{
		m_awarenessSphere->SetSphereRadius(GreetingRange);
		m_awarenessSphere->OnComponentBeginOverlap.AddDynamic(
			this, &ANPCCharacter::OnAwarenessSphereBeginOverlap);
	}

	InitializeECS();

	if (m_nameTag)
		m_nameTag->SetText(FText::FromString(GetNPCName()));


	if (FInventoryData* inv = GetInventory())
		inv->Gold = StartingGold;

	LoadTestSchedule();   

	if (bUseLLMSchedule)
		RequestLLMSchedule(true);   // erster Plan → Cooldown umgehen

}

void ANPCCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (m_conversationPartner.IsValid())
		EndConversation();   // Partner freigeben, sonst hängt der im Talk-State fest
	StopSpotAnimation();
	ReleaseCurrentWorkstation();
	Super::EndPlay(EndPlayReason);
}


void ANPCCharacter::ApplyModularAppearance()
{
	if (!NPCData) return;

	USkeletalMeshComponent* body = GetMesh();
	if (!body) return;

	// Optionaler Basiskörper aus dem DataAsset (sonst bleibt das BP-Mesh).
	if (NPCData->BodyMesh)
		body->SetSkeletalMeshAsset(NPCData->BodyMesh);

	// Passende Anim-Blueprint zum Skelett dieses NPCs (z.B. männlich/weiblich).
	if (NPCData->AnimClass)
		body->SetAnimInstanceClass(NPCData->AnimClass);

	for (const TObjectPtr<USkeletalMesh>& piece : NPCData->OutfitPieces)
	{
		if (!piece) continue;

		USkeletalMeshComponent* comp = NewObject<USkeletalMeshComponent>(this);
		if (!comp) continue;

		comp->SetSkeletalMeshAsset(piece);
		comp->RegisterComponent();
		comp->AttachToComponent(body, FAttachmentTransformRules::KeepRelativeTransform);

		comp->SetLeaderPoseComponent(body);

		m_outfitComponents.Add(comp);
	}

	for (const FCharacterAttachment& att : NPCData->Attachments)
	{
		if (!att.Mesh) continue;

		UStaticMeshComponent* comp = NewObject<UStaticMeshComponent>(this);
		if (!comp) continue;

		comp->SetStaticMesh(att.Mesh);
		comp->RegisterComponent();
		comp->AttachToComponent(body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, att.Socket);
		comp->SetRelativeTransform(att.RelativeTransform);   

		m_attachmentComponents.Add(comp);
	}
}

void ANPCCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (m_nameTag && m_nameTag->IsVisible())
	{
		if (APlayerCameraManager* cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FRotator look = (cam->GetCameraLocation() - m_nameTag->GetComponentLocation()).Rotation();
			m_nameTag->SetWorldRotation(FRotator(0.0f, look.Yaw, 0.0f));
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UWorldClock* clock = World->GetSubsystem<UWorldClock>())
		{
			FScheduleSystem::CurrentGameHour = clock->GetHour();

			// Spielstunden pro Echtzeit-Sekunde → Needs rechnen in Spielzeit
			const float rsph = clock->RealSecondsPerGameHour;
			FScheduleSystem::GameHoursPerSecond = (rsph > 0.0f) ? (1.0f / rsph) : 0.0f;

			// Tägliche Neuplanung: neuer Spieltag → frischen LLM-Plan anfragen.
			if (bUseLLMSchedule && clock->GetDay() != m_lastScheduleDay)
			{
				m_lastScheduleDay = clock->GetDay();
				UE_LOG(LogTemp, Log, TEXT("[%s] Neuer Spieltag %d (%s) – plane Tagesablauf neu"),
					*GetNPCName(), m_lastScheduleDay, *clock->GetWeekdayName());
				RequestLLMSchedule(true);   // Tageswechsel → Cooldown umgehen
			}
		}
	}

	if (FWaypointData* wp = GetWaypoints())
		wp->bHasSpot = m_reservedSpot.IsValid();

	if (FScheduleData* sched = GetSchedule())
	{
		const AMarketStall* stall = m_assignedStall.Get();
		sched->bVendorOnDuty = stall && stall->IsOpenNow() && stall->HasVendorStand();
	}

	// ECS updaten
	m_ecs.Update(DeltaTime);
	SyncGoalToBlackboard();

	// NPC-zu-NPC-Gespräche: automatisch anstoßen (außer der BT übernimmt das → Flag aus).
	if (m_socialCooldown > 0.0f)
		m_socialCooldown -= DeltaTime;
	else if (bAutoStartConversations)
		TryStartNPCConversation();

	// Beziehungen ohne Auffrischung langsam Richtung neutral verblassen lassen.
	if (RelationshipDecayPerHour > 0.0f)
		if (FRelationshipData* rel = GetRelationships())
			rel->DecayTowardNeutral(RelationshipDecayPerHour * FScheduleSystem::GameHoursPerSecond * DeltaTime);
}

// Bodentrace am Punkt: liegt dort das gemiedene Physical Material (Gras-Layer)?
static bool WanderPointIsOnSurface(UWorld* _world, const FVector& _point, UPhysicalMaterial* _surface)
{
	if (!_world || !_surface) return false;

	FHitResult hit;
	FCollisionQueryParams params(TEXT("WanderSurface"), /*bTraceComplex*/ true);
	params.bReturnPhysicalMaterial = true;

	const FVector start = _point + FVector(0.0f, 0.0f, 250.0f);
	const FVector end   = _point - FVector(0.0f, 0.0f, 250.0f);
	if (_world->LineTraceSingleByChannel(hit, start, end, ECC_Visibility, params))
		return hit.PhysMaterial.Get() == _surface;

	return false;
}

bool ANPCCharacter::FindWanderTarget(FVector& _out) const
{
	UWorld* world = GetWorld();
	UNavigationSystemV1* nav = world ? UNavigationSystemV1::GetCurrent(world) : nullptr;
	if (!nav) return false;

	const FVector origin = GetActorLocation();
	const float minDistSq = FMath::Square(WanderRadius * 0.5f);


	bool    bHaveMarket    = false;
	FVector marketCenter   = FVector::ZeroVector;
	const float marketAvoidSq = FMath::Square(MarketAvoidRadius);
	if (MarketAvoidRadius > 0.0f)
		if (UGameInstance* gi = GetGameInstance())
			if (UWorldLocationManager* lm = gi->GetSubsystem<UWorldLocationManager>())
			{
				FWorldLocation mkt;
				if (lm->GetLocationByTag(EWorldLocationTag::Market, mkt))
				{
					bHaveMarket  = true;
					marketCenter = mkt.Position;
				}
			}

	bool    bAny = false;
	FVector anyReachable = origin;

	for (int32 i = 0; i < 12; ++i)
	{
		FNavLocation rnd;
		if (!nav->GetRandomReachablePointInRadius(origin, WanderRadius, rnd))
			continue;

		bAny = true;
		anyReachable = rnd.Location;   // Fallback

		const bool bFarEnough  = FVector::DistSquared(origin, rnd.Location) >= minDistSq;
		const bool bOnGrass    = (WanderAvoidSurface && WanderPointIsOnSurface(world, rnd.Location, WanderAvoidSurface));
		const bool bNearMarket = (bHaveMarket && FVector::DistSquared(rnd.Location, marketCenter) < marketAvoidSq);

		if (bFarEnough && !bOnGrass && !bNearMarket)
		{
			_out = rnd.Location;
			return true;
		}
	}

	if (bAny) { _out = anyReachable; return true; }
	return false;
}

void ANPCCharacter::ChooseNextDestination()
{
	UGameInstance* GI = GetGameInstance();
    if (!GI) return;
    UWorldLocationManager* locMgr = GI->GetSubsystem<UWorldLocationManager>();
    if (!locMgr) return;

    FNeedsData*    needs    = GetNeeds();
    FNavMeshData*  navMesh  = GetNavMesh();
    FWaypointData* waypoint = GetWaypoints();
    FIdentityData* identity = GetIdentity();
    if (!needs || !navMesh || !waypoint || !identity) return;

    ENPCOccupation occupation = NPCData ? NPCData->Occupation : ENPCOccupation::Villager;
    ENPCDesire goal = waypoint->CurrentGoal;

    if (goal != ENPCDesire::Shop && m_reservedStall.IsValid())
    {
        m_reservedStall->ReleaseCustomer(GetEntityID());
        m_reservedStall.Reset();
    }

    // Idle/Talk → kein Ortswechsel 
    if (goal == ENPCDesire::Idle || goal == ENPCDesire::Talk)
        return;

    FWorldLocation chosen;
    bool bFound = false;

    // ── Work/Sleep: platzierten Spot-Actor (Amboss/Bett) bevorzugen ──
    AWorldSpot*       spot    = nullptr;
    EWorldLocationTag spotTag = EWorldLocationTag::None;

    if (goal == ENPCDesire::Work)
    {
        if (AMarketStall* stall = m_assignedStall.Get())
        {
            if (stall->IsOpenNow() && stall->HasVendorStand())
            {
                navMesh->SetTargetLocation(stall->GetVendorStandTransform().GetLocation(),
                                           TEXT("Market"));
                navMesh->AcceptRadius = 10.0f;   // präzise an die Standposition
                m_currentLocationTag  = EWorldLocationTag::Market;
                m_reservedSpot.Reset();          // kein AWorldSpot – Snap via TrySnapToStallStand
                return;
            }
        }

        spotTag = identity->WorkTag;
        spot = locMgr->ReserveSpotActor(spotTag, ESpotType::Work, GetEntityID());
    }
    else if (goal == ENPCDesire::Shop)
    {
        // Einkaufen: nächsten GEÖFFNETEN Stand mit freiem Kundenplatz suchen und dort
        // den Kundenplatz einnehmen (Position + Blickrichtung kommen vom CustomerSlot).
        AMarketStall* bestStall = nullptr;
        float bestSq = TNumericLimits<float>::Max();
        const FVector from = GetActorLocation();
        for (TActorIterator<AMarketStall> it(GetWorld()); it; ++it)
        {
            AMarketStall* s = *it;
            if (!s || !s->IsOpenNow() || !s->HasCustomerStand()) continue;
            if (!s->IsCustomerSlotFreeFor(GetEntityID()))         continue;
            const float dSq = FVector::DistSquared(from, s->GetActorLocation());
            if (dSq < bestSq) { bestSq = dSq; bestStall = s; }
        }

        if (bestStall && bestStall->TryReserveCustomer(GetEntityID()))
        {
            // vorherige Reservierung an einem anderen Stand freigeben
            if (m_reservedStall.IsValid() && m_reservedStall.Get() != bestStall)
                m_reservedStall->ReleaseCustomer(GetEntityID());
            m_reservedStall = bestStall;

            navMesh->SetTargetLocation(bestStall->GetCustomerStandTransform().GetLocation(), TEXT("Market"));
            navMesh->AcceptRadius = 10.0f;
            m_currentLocationTag  = EWorldLocationTag::Market;
            m_reservedSpot.Reset();   
            return;
        }

        return;
    }
    else if (goal == ENPCDesire::Sleep)
    {
        // Zuerst im eigenen Zuhause (HomeTag), sonst direkt die Taverne.
        spotTag = (identity->HomeTag != EWorldLocationTag::None)
                      ? identity->HomeTag : EWorldLocationTag::Tavern;
        spot = locMgr->ReserveSpotActor(spotTag, ESpotType::Sleep, GetEntityID());

        // Kein Bett im eigenen Haus frei → in der Taverne nach freien Betten suchen.
        if (!spot && spotTag != EWorldLocationTag::Tavern)
        {
            spotTag = EWorldLocationTag::Tavern;
            spot    = locMgr->ReserveSpotActor(spotTag, ESpotType::Sleep, GetEntityID());
        }
    }
    else if (goal == ENPCDesire::Rest)
    {
        // Erholungs-Spot (Bank/Stuhl) am HomeTag, sonst Taverne …
        spotTag = (identity->HomeTag != EWorldLocationTag::None)
                      ? identity->HomeTag : EWorldLocationTag::Tavern;
        spot = locMgr->ReserveSpotActor(spotTag, ESpotType::Rest, GetEntityID());

        if (!spot && spotTag != EWorldLocationTag::Tavern)
            spot = locMgr->ReserveSpotActor(EWorldLocationTag::Tavern, ESpotType::Rest, GetEntityID());

        // … sonst einfach die NÄCHSTGELEGENE freie Bank (egal welches Gebäude).
        if (!spot)
            spot = locMgr->ReserveNearestSpotActor(ESpotType::Rest, GetActorLocation(), GetEntityID());

        if (spot)
            UE_LOG(LogTemp, Log, TEXT("[%s] Rest-Spot reserviert: %s"), *GetNPCName(), *spot->GetName());
    }
    else if (goal == ENPCDesire::Eat || goal == ENPCDesire::Drink)
    {
        // Hat der NPC passendes Essen/Trinken DABEI? → an einem Rest-Platz verzehren,
        // statt in die Taverne zu laufen.
        m_pendingConsume = NAME_None;
        const EConsumeType want = (goal == ENPCDesire::Drink) ? EConsumeType::Drink : EConsumeType::Food;
        if (UItemDataAsset* food = FindConsumable(want))
        {
            spotTag = (identity->HomeTag != EWorldLocationTag::None)
                          ? identity->HomeTag : EWorldLocationTag::Tavern;
            spot = locMgr->ReserveSpotActor(spotTag, ESpotType::Rest, GetEntityID());
            if (!spot)
                spot = locMgr->ReserveNearestSpotActor(ESpotType::Rest, GetActorLocation(), GetEntityID());

            if (spot)
            {
                m_pendingConsume = food->ItemId;   // wird in PerformAction am Spot verzehrt
            }
        }

        // Kein Essen dabei (oder kein Rest-Platz frei) → Essplatz in der Taverne.
        if (!spot)
        {
            spotTag = EWorldLocationTag::Tavern;
            spot = locMgr->ReserveSpotActor(spotTag, ESpotType::Eat, GetEntityID());
            if (!spot)
                spot = locMgr->ReserveNearestSpotActor(ESpotType::Eat, GetActorLocation(), GetEntityID());

            if (spot)
            {
                const UEnum* goalEnum = StaticEnum<ENPCDesire>();
            }
        }
    }

    if (spot)
    {
        navMesh->SetTargetLocation(spot->GetStandTransformFor(GetEntityID()).GetLocation(), WorldLocationTagToString(spot->Building));
        navMesh->AcceptRadius = 10.0f;  
        m_currentLocationTag = spot->Building;
        m_reservedSpot       = spot;

        return;   
    }

    if (goal == ENPCDesire::Work)
    {
        // 0) Bedürfnis vorziehen: schon spürbar hungrig/müde? Dann jetzt erledigen.
        //    Der Override schaltet ab dem nächsten Frame auf Eat/Rest.
        if (needs->OverrideState == ENeedOverride::None)
        {
            if (needs->Hunger >= needs->HungerThreshhold * 0.7f)
            {
                needs->OverrideState = ENeedOverride::Eat;
            }
            else if (needs->Exhaustion >= needs->ExhaustionThreshhold * 0.7f)
            {
                needs->OverrideState = ENeedOverride::Rest;
            }
        }

        // 1) Hilfsarbeit: freier Idle-/Prep-Spot am Arbeitsgebäude (Kohle holen,
        //    Werkzeug sortieren, Feuer schüren …). Sichtbare Tätigkeit statt Leerlauf.
        if (AWorldSpot* prep = locMgr->ReserveSpotActor(identity->WorkTag, ESpotType::Idle, GetEntityID()))
        {
            navMesh->SetTargetLocation(prep->GetStandTransformFor(GetEntityID()).GetLocation(),
                                       WorldLocationTagToString(prep->Building));
            navMesh->AcceptRadius = 10.0f;
            m_currentLocationTag  = prep->Building;
            m_reservedSpot        = prep;
            return;
        }

        // 2) Sozialisieren: kurz zu einem benannten Ort (Markt/Taverne/Brunnen …)
        if (FMath::FRand() < WorkBusySocializeChance &&
            locMgr->GetRandomLocation(m_currentLocationTag, chosen))
        {
            navMesh->SetTargetLocation(chosen.Position, WorldLocationTagToString(chosen.Tag));
            navMesh->AcceptRadius = 60.0f;
            m_currentLocationTag  = chosen.Tag;            
			m_reservedSpot.Reset();
            return;
        }

        // 3) In der Nähe warten (Zufallspunkt im Umkreis), dann erneut prüfen.
        if (UNavigationSystemV1* navSys = UNavigationSystemV1::GetCurrent(GetWorld()))
        {
            FNavLocation rnd;
            if (navSys->GetRandomReachablePointInRadius(GetActorLocation(), 400.0f, rnd))
            {
                navMesh->SetTargetLocation(rnd.Location, TEXT("WaitForWork"));
                navMesh->AcceptRadius = 60.0f;
                m_currentLocationTag  = EWorldLocationTag::None;                m_reservedSpot.Reset();
                UE_LOG(LogTemp, Log, TEXT("[%s] Alle Arbeitsplaetze besetzt – wartet in der Naehe"),
                       *GetNPCName());
                return;
            }
        }
    }


    if (goal == ENPCDesire::Wander)
    {
        FVector wanderTarget;
        if (FindWanderTarget(wanderTarget))
        {
            navMesh->SetTargetLocation(wanderTarget, TEXT("Wander"));
            navMesh->AcceptRadius = 60.0f;
            m_currentLocationTag  = EWorldLocationTag::None;
            m_reservedSpot.Reset();
            UE_LOG(LogTemp, Log, TEXT("[%s] wandert zu Nicht-Gras-Punkt"), *GetNPCName());
            return;
        }
        return;
    }

    if (goal == ENPCDesire::Rest)
    {
        bFound = locMgr->GetRandomLocation(m_currentLocationTag, chosen);
    }

    if (goal == ENPCDesire::Sleep && spotTag != EWorldLocationTag::None)
        bFound = locMgr->GetLocationByTag(spotTag, chosen);

    if (!bFound)
        bFound = locMgr->GetLocationForDesire(goal, occupation, chosen);
    if (!bFound)
        bFound = locMgr->GetRandomLocation(m_currentLocationTag, chosen);
    if (!bFound) return;


    FVector targetPos = chosen.Position;
    if (UNavigationSystemV1* navSys = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation scattered;
        if (navSys->GetRandomReachablePointInRadius(chosen.Position, 350.0f, scattered))
            targetPos = scattered.Location;
    }

    navMesh->SetTargetLocation(targetPos, WorldLocationTagToString(chosen.Tag));
    navMesh->AcceptRadius = 50.0f;   
    m_currentLocationTag = chosen.Tag;
    m_reservedSpot.Reset();   

    const UEnum* E = StaticEnum<ENPCDesire>();
}

void ANPCCharacter::ReleaseCurrentWorkstation()
{
	if (AWorldSpot* spot = m_reservedSpot.Get())
		spot->Release(GetEntityID());

	m_reservedSpot.Reset();

	// Kundenplatz am Marktstand ebenfalls freigeben (falls einer reserviert war).
	if (AMarketStall* stall = m_reservedStall.Get())
		stall->ReleaseCustomer(GetEntityID());
	m_reservedStall.Reset();
}

// ============================================================================
//  Spot-Animationen: Intro (einmal) → Loop. 
// ============================================================================
void ANPCCharacter::PlaySpotAnimation(AWorldSpot* _spot)
{
	if (!_spot) return;
	UAnimInstance* anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

	// Montage passend zur Tätigkeit wählen (Trinken → DrinkMontage falls gesetzt).
	const ENPCDesire goal = GetWaypoints() ? GetWaypoints()->CurrentGoal : ENPCDesire::None;
	UAnimMontage* montage = _spot->GetMontageForGoal(goal);
	m_spotMontage  = montage;
	m_bOutroPlayed = false;   
	GetWorldTimerManager().ClearTimer(m_loopActiveTimer);

	if (!anim)
	{
		SetSpotLoopActive(true);   
		return;
	}
	if (!montage)
	{
		SetSpotLoopActive(true);
		return;
	}

	// Kombinierte Montage von vorne: Intro → Loop (loopt). Das Outro kommt später
	// per JumpToSection 
	const float len = anim->Montage_Play(montage);

	// Needs erst ab der Loop-Section zählen: Timer über die Länge der Intro-Section.
	// Keine Intro-Section gefunden → sofort zählen.
	SetSpotLoopActive(false);
	ClearIntroOffset();   
	float introLen = 0.f;
	const int32 introIdx = montage->GetSectionIndex(_spot->IntroSection);
	if (introIdx != INDEX_NONE)
	{
		float s = 0.f, e = 0.f;
		montage->GetSectionStartAndEndTime(introIdx, s, e);
		introLen = e - s;
	}
	if (introLen > 0.f)
	{
		// Intro-Offset NUR während der Intro-Section; beim Loop wieder zurücknehmen.
		ApplyIntroOffset(_spot->IntroMeshOffset);
		GetWorldTimerManager().SetTimer(m_loopActiveTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { ClearIntroOffset(); SetSpotLoopActive(true); }),
			introLen, false);
	}
	else
	{
		SetSpotLoopActive(true);
	}
}

void ANPCCharacter::PlaySpotOutro()
{
	if (m_bOutroPlayed) return;   // pro Spot-Besuch nur EINMAL auslösen
	m_bOutroPlayed = true;

	GetWorldTimerManager().ClearTimer(m_loopActiveTimer);
	ClearIntroOffset();           
	SetSpotLoopActive(false);     

	UAnimInstance* anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	AWorldSpot*    spot = m_reservedSpot.Get();
	UAnimMontage*  montage = m_spotMontage.Get();

	// In die Outro-Section springen – SELBE Montage, also lückenlos (kein Stop/Start).
	if (anim && montage && spot && anim->Montage_IsPlaying(montage) &&
		montage->GetSectionIndex(spot->OutroSection) != INDEX_NONE)
	{
		anim->Montage_JumpToSection(spot->OutroSection, montage);
	}
	else
	{
		StopSpotAnimation();
	}
}

bool ANPCCharacter::IsPlayingOutro() const
{
	const UAnimInstance* anim    = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	const AWorldSpot*    spot    = m_reservedSpot.Get();
	const UAnimMontage*  montage = m_spotMontage.Get();
	return anim && montage && spot && anim->Montage_IsPlaying(montage)
		&& anim->Montage_GetCurrentSection(montage) == spot->OutroSection;
}

void ANPCCharacter::StopSpotAnimation()
{
	GetWorldTimerManager().ClearTimer(m_loopActiveTimer);
	ClearIntroOffset();
	SetSpotLoopActive(false);

	if (UAnimInstance* anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		if (UAnimMontage* montage = m_spotMontage.Get())
			anim->Montage_Stop(0.25f, montage);   
	m_spotMontage.Reset();
}

void ANPCCharacter::SetSpotLoopActive(bool _bActive)
{
	if (FWaypointData* wp = GetWaypoints())
		wp->bLoopActive = _bActive;
}

void ANPCCharacter::ApplyIntroOffset(const FVector& _offset)
{
	if (m_bIntroOffsetActive || _offset.IsNearlyZero()) return;
	if (USkeletalMeshComponent* mesh = GetMesh())
	{
		mesh->AddRelativeLocation(_offset);
		m_appliedIntroOffset = _offset;
		m_bIntroOffsetActive = true;
	}
}

void ANPCCharacter::ClearIntroOffset()
{
	if (!m_bIntroOffsetActive) return;
	if (USkeletalMeshComponent* mesh = GetMesh())
		mesh->AddRelativeLocation(-m_appliedIntroOffset);
	m_appliedIntroOffset = FVector::ZeroVector;
	m_bIntroOffsetActive = false;
}

bool ANPCCharacter::CanInteract() const
{
	if (!m_bPlayerInRange) return false;

	// Schlafende NPCs sind nicht ansprechbar (erst aufwachen).
	if (const FWaypointData* wp = GetComponent<FWaypointData>())
		if (wp->CurrentGoal == ENPCDesire::Sleep)
			return false;

	return true;
}

void ANPCCharacter::OnInteract(AActor* _interactor)
{
	// Redet der NPC gerade mit einem anderen NPC? Erst dieses Gespräch beenden,
	// dann dem Spieler zuwenden (Spieler hat Vorrang).
	if (m_conversationPartner.IsValid())
		EndConversation();

	SetTalking(true, _interactor);   
	m_socialCooldown = SocialCooldown;   

}

void ANPCCharacter::SetHighlighted(bool _bOn)
{

	const int32 stencil = _bOn ? HighlightStencil : 0;

	auto applyOutline = [_bOn, stencil](UPrimitiveComponent* _comp)
	{
		if (!_comp) return;
		_comp->SetRenderCustomDepth(_bOn);
		_comp->SetCustomDepthStencilValue(stencil);
	};

	applyOutline(GetMesh());

	for (const TObjectPtr<USkeletalMeshComponent>& piece : m_outfitComponents)
		applyOutline(piece);

	for (const TObjectPtr<UStaticMeshComponent>& att : m_attachmentComponents)
		applyOutline(att);
}

void ANPCCharacter::SetTalking(bool _bTalking, AActor* _interactor)
{
	m_talkTarget = _bTalking ? _interactor : nullptr;

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
		{
			BB->SetValueAsBool(FNPCBlackboardKeys::PlayerTalking, _bTalking);
		}
	}

	if (_bTalking)
	{
		m_bTalkReplying = false;
		PlayCurrentTalkMontage();
	}
	else
	{
		StopTalkAnimation();
	}

}

void ANPCCharacter::PlayTalkReply()
{
	m_bTalkReplying        = true;
	m_bTalkReplyEndPending = false;   
	PlayCurrentTalkMontage();
}

void ANPCCharacter::EndTalkReply()
{
	if (!m_bTalkReplying) return;

	UAnimInstance* anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (anim && TalkReplyMontage && anim->Montage_IsPlaying(TalkReplyMontage))
	{
		m_bTalkReplyEndPending = true;
		return;
	}


	m_bTalkReplying        = false;
	m_bTalkReplyEndPending = false;
	if (m_talkTarget.IsValid())
		PlayCurrentTalkMontage();
}

void ANPCCharacter::PlayCurrentTalkMontage()
{
	UAnimInstance* anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!anim) return;

	UAnimMontage* montage = m_bTalkReplying ? TalkReplyMontage : TalkIdleMontage;
	UAnimMontage* other   = m_bTalkReplying ? TalkIdleMontage : TalkReplyMontage;

	if (other && anim->Montage_IsPlaying(other))
		anim->Montage_Stop(0.15f, other);

	if (!montage) return;

	const float len = anim->Montage_Play(montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f,
		/*bStopAllMontages*/ false);
	if (len <= 0.f) return;  


	FOnMontageBlendingOutStarted del;
	del.BindUObject(this, &ANPCCharacter::OnTalkMontageBlendingOut);
	anim->Montage_SetBlendingOutDelegate(del, montage);
}

void ANPCCharacter::OnTalkMontageBlendingOut(UAnimMontage* _montage, bool _bInterrupted)
{

	if (_bInterrupted || !m_talkTarget.IsValid()) return;

	const UAnimMontage* current = m_bTalkReplying ? TalkReplyMontage : TalkIdleMontage;
	if (_montage != current) return;

	if (m_bTalkReplying && m_bTalkReplyEndPending)
	{
		m_bTalkReplying        = false;
		m_bTalkReplyEndPending = false;
	}

	PlayCurrentTalkMontage(); 
}

void ANPCCharacter::StopTalkAnimation()
{
	m_bTalkReplying        = false;
	m_bTalkReplyEndPending = false;
	UAnimInstance* anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!anim) return;

	anim->StopAllMontages(0.1f);
}


bool ANPCCharacter::PauseMovementForDoor()
{
	AAIController* aiCon = Cast<AAIController>(GetController());
	if (!aiCon) return false;

	UPathFollowingComponent* pfc = aiCon->GetPathFollowingComponent();
	if (!pfc || pfc->GetStatus() != EPathFollowingStatus::Moving)
		return false;   

	pfc->PauseMove();
	if (UCharacterMovementComponent* move = GetCharacterMovement())
		move->StopMovementImmediately();


	if (DoorGestureMontage)
		PlayAnimMontage(DoorGestureMontage);

	return true;
}

void ANPCCharacter::ResumeMovementFromDoor()
{
	if (AAIController* aiCon = Cast<AAIController>(GetController()))
		if (UPathFollowingComponent* pfc = aiCon->GetPathFollowingComponent())
			pfc->ResumeMove(FAIRequestID::CurrentRequest);
}

FString ANPCCharacter::GetInteractionPrompt()
{
	return GenerateHermesPrompt();
}

// ============================================================================
//  Wirtschaft (Inventar + Gold) – dünne Wrapper auf die ECS-Komponente.
// ============================================================================
int32 ANPCCharacter::GetGold() const
{
	const FInventoryData* inv = GetComponent<FInventoryData>();
	return inv ? inv->Gold : 0;
}

void ANPCCharacter::AddGold(int32 _amount)
{
	if (FInventoryData* inv = GetInventory()) inv->AddGold(_amount);
}

bool ANPCCharacter::SpendGold(int32 _amount)
{
	FInventoryData* inv = GetInventory();
	return inv ? inv->SpendGold(_amount) : false;
}

void ANPCCharacter::AddItem(FName _itemId, int32 _count)
{
	if (FInventoryData* inv = GetInventory()) inv->AddItem(_itemId, _count);
}

bool ANPCCharacter::RemoveItem(FName _itemId, int32 _count)
{
	FInventoryData* inv = GetInventory();
	return inv ? inv->RemoveItem(_itemId, _count) : false;
}

int32 ANPCCharacter::GetItemCount(FName _itemId) const
{
	const FInventoryData* inv = GetComponent<FInventoryData>();
	return inv ? inv->GetItemCount(_itemId) : 0;
}

TArray<FInventorySlot> ANPCCharacter::GetInventorySlots() const
{
	const FInventoryData* inv = GetComponent<FInventoryData>();
	if (!inv) return {};

	if (const UWorld* w = GetWorld())
		if (const ALLM_NPCGameMode* gm = w->GetAuthGameMode<ALLM_NPCGameMode>())
			return gm->MakeSlots(*inv);


	TArray<FInventorySlot> slots;
	for (const TPair<FName, int32>& pair : inv->Items)
	{
		FInventorySlot s;
		s.ItemId      = pair.Key;
		s.Count       = pair.Value;
		s.DisplayName = pair.Key.ToString();
		slots.Add(s);
	}
	return slots;
}

// ============================================================================
//  Konsumierbare Items (Essen/Trinken aus dem Inventar)
// ============================================================================
UItemDataAsset* ANPCCharacter::FindConsumable(EConsumeType _type) const
{
	const FInventoryData* inv = GetComponent<FInventoryData>();
	if (!inv) return nullptr;

	const UWorld* w = GetWorld();
	const ALLM_NPCGameMode* gm = w ? w->GetAuthGameMode<ALLM_NPCGameMode>() : nullptr;
	if (!gm) return nullptr;   

	for (const TPair<FName, int32>& pair : inv->Items)
	{
		if (pair.Value <= 0) continue;
		if (UItemDataAsset* def = gm->FindItem(pair.Key))
			if (def->bConsumable && def->ConsumeType == _type)
				return def;
	}
	return nullptr;
}

bool ANPCCharacter::ConsumeItem(FName _itemId)
{
	if (_itemId.IsNone()) return false;

	const UWorld* w = GetWorld();
	const ALLM_NPCGameMode* gm = w ? w->GetAuthGameMode<ALLM_NPCGameMode>() : nullptr;
	UItemDataAsset* def = gm ? gm->FindItem(_itemId) : nullptr;
	if (!def || !def->bConsumable) return false;

	if (!RemoveItem(_itemId, 1)) return false;  

	if (FNeedsData* needs = GetNeeds())
	{
		if (def->ConsumeType == EConsumeType::Food)
			needs->Hunger     = FMath::Max(0.0f, needs->Hunger     - def->Nourishment);
		else
			needs->Exhaustion = FMath::Max(0.0f, needs->Exhaustion - def->Nourishment);
	}

	return true;
}

void ANPCCharacter::TryConsumePending()
{
	if (!m_pendingConsume.IsNone())
	{
		ConsumeItem(m_pendingConsume);
		m_pendingConsume = NAME_None;
	}
}

// ============================================================================
//  Marktstand (Verkäufer)
// ============================================================================
void ANPCCharacter::AdjustPlayerLiking(float _delta)
{
	m_playerLiking = FMath::Clamp(m_playerLiking + _delta, 0.0f, 1.0f);
}

float ANPCCharacter::GetLikingToward(uint32 _entityId) const
{
	if (const FRelationshipData* rel = GetComponent<FRelationshipData>())
		if (const FNPCRelation* r = rel->FindRelation(_entityId))
			return r->Liking;
	return 0.5f;   
}

void ANPCCharacter::SetAssignedStall(AMarketStall* _stall)
{
	m_assignedStall = _stall;
}

AMarketStall* ANPCCharacter::GetAssignedStall() const
{
	return m_assignedStall.Get();
}

bool ANPCCharacter::TrySnapToStallStand()
{
	const AMarketStall* stall = nullptr;
	FTransform st;

	// Verkäufer an seinem offenen Stand: IMMER an den VendorSlot 
	if (const AMarketStall* mine = m_assignedStall.Get())
		if (mine->IsOpenNow() && mine->HasVendorStand())
		{
			stall = mine;
			st    = mine->GetVendorStandTransform();
		}

	// Sonst: Käufer an seinem reservierten Kundenplatz.
	if (!stall)
	{
		const FWaypointData* wp = GetWaypoints();
		if (wp && wp->CurrentGoal == ENPCDesire::Shop)
			if (const AMarketStall* cust = m_reservedStall.Get())
				if (cust->IsOpenNow() && cust->HasCustomerStand())
				{
					stall = cust;
					st    = cust->GetCustomerStandTransform();
				}
	}

	if (!stall) return false;

	FVector loc = st.GetLocation();
	loc.Z = GetActorLocation().Z;
	SetActorLocationAndRotation(loc, st.Rotator());
	return true;
}

void ANPCCharacter::InitializeECS()
{

	m_entityID = m_ecs.CreateEntity(this);


	FNPCEntity* entity = m_ecs.GetEntity(m_entityID);


	// ================================================================
	//  DATAASSET 
	// ================================================================
	if (NPCData)
	{
		// --- Identity ---
		entity->Identity.Name = NPCData->NPCName;
		entity->Identity.Role = NPCData->GetOccupationString();
		entity->Identity.Personality = NPCData->Personality;
		entity->Identity.Goals = NPCData->Goals;
		entity->Identity.HomeTag = NPCData->HomeTag;
		entity->Identity.WorkTag = NPCData->WorkTag;

		// --- Needs ---
		entity->Needs.Hunger = NPCData->BaseHunger;
		entity->Needs.Exhaustion = NPCData->BaseExhaustion;

		// Nacht-/Wachfenster am Schlaf-Typ ausrichten
		entity->Needs.SleepHourThreshold = SleepHourForType(NPCData->SleepType);
		entity->Needs.WakeHourThreshold  = WakeHourForType(NPCData->SleepType);

		entity->ActiveSystemFlags = NPCData->ActiveSystems;
	}
	// ================================================================
	//  OHNE DATAASSET — Fallback
	// ================================================================
	else
	{
		entity->Identity.Name = GetActorNameOrLabel();
		entity->Identity.Role = TEXT("NPC");
		entity->Identity.Personality = TEXT("Unknown");

		entity->Needs.Hunger = 0.3f;
		entity->Needs.Exhaustion = 0.2f;

		entity->ActiveSystemFlags =
			static_cast<int32>(ESystemType::MemoryDecay) |
			static_cast<int32>(ESystemType::EmotionDecay) |
			static_cast<int32>(ESystemType::Needs) |
			static_cast<int32>(ESystemType::TransformSync);
	}

	// --- Transform (immer vom Actor, nie vom DataAsset) ---
	entity->Transform.OwnerActor = this;
	entity->Transform.Location = GetActorLocation();
	entity->Transform.Rotation = GetActorRotation();

	// --- NavMesh ---
	entity->NavMesh.AcceptRadius = 50.0f;

	UE_LOG(LogTemp, Warning,
	       TEXT("[ECS] %s initialized — Entity ID: %d"),
	       *entity->Identity.Name, m_entityID);
}

// ============================================================
//  OVERLAP CALLBACKS
// ============================================================
void ANPCCharacter::OnInteractionSphereBeginOverlap(UPrimitiveComponent* _overlappedComp, AActor* _otherActor,
                                                    UPrimitiveComponent* _otherComp, int32 _otherBodyIndex,
                                                    bool _bFromSweep, const FHitResult& _sweepResult)
{
	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(_otherActor))
	{
		m_bPlayerInRange = true;
		player->AddNearbyInteractable(this);  
		UE_LOG(LogTemp, Warning, TEXT("[NPC] %s: Player in range"), *GetNPCName());

		if (m_nameTag)
		{
			m_nameTag->SetText(FText::FromString(GetNPCName()));
			m_nameTag->SetVisibility(true);
		}

		// Fallback
		RequestGreeting();
	}
}

void ANPCCharacter::OnAwarenessSphereBeginOverlap(UPrimitiveComponent* _overlappedComp, AActor* _otherActor,
                                                  UPrimitiveComponent* _otherComp, int32 _otherBodyIndex,
                                                  bool _bFromSweep, const FHitResult& _sweepResult)
{
	if (Cast<ALLM_NPCCharacter>(_otherActor))
		RequestGreeting();
}

void ANPCCharacter::OnInteractionSphereEndOverlap(UPrimitiveComponent* _overlappedComp, AActor* _otherActor,
                                                  UPrimitiveComponent* _otherComp, int32 _otherBodyIndex)
{
	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(_otherActor))
	{
		m_bPlayerInRange = false;
		player->RemoveNearbyInteractable(this);

		if (!m_conversationPartner.IsValid())
			SetTalking(false);

		if (m_nameTag)
			m_nameTag->SetVisibility(false);
	}
}

void ANPCCharacter::SyncGoalToBlackboard()
{
	FWaypointData* wp = GetWaypoints();
	if (!wp) return;

	const UEnum* E = StaticEnum<ENPCDesire>();

	if (wp->CurrentGoal != m_lastGoal)
	{
		wp->bIsPausing = false;
	}

	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC) return;
	UBlackboardComponent* BB = AIC->GetBlackboardComponent();
	if (!BB) return;

	FString goalStr = E ? E->GetNameStringByValue((int64)wp->CurrentGoal) : TEXT("Wander");
	int32 sepIdx;
	if (goalStr.FindLastChar(TEXT(':'), sepIdx))
		goalStr = goalStr.RightChop(sepIdx + 1);

	BB->SetValueAsString(FNPCBlackboardKeys::CurrentAction, goalStr);
	BB->SetValueAsBool(TEXT("IsSleeping"), wp->CurrentGoal == ENPCDesire::Sleep);
}


void ANPCCharacter::LoadTestSchedule()
{
	FScheduleData* sched = GetSchedule();
	if (!sched) return;

	sched->DailySchedule.Empty();

	auto Add = [&](int32 h, const FString& a)
	{
		FDayPlaynEntry e; e.Hour = h; e.Activity = a;
		sched->DailySchedule.Add(e);
	};
	Add(6, TEXT("work"));  Add(12, TEXT("eat"));  Add(13, TEXT("work"));
	Add(19, TEXT("eat"));  Add(22, TEXT("sleep"));

	sched->DailySchedule.Sort([](const FDayPlaynEntry& A, const FDayPlaynEntry& B)
	{
		return A.Hour < B.Hour;
	});

	sched->CurrentActivityIndex = -1;
	sched->bScheduleReady = true;
}

void ANPCCharacter::ReplanSchedule(bool _bForce)
{
	if (!bUseLLMSchedule)
	{
		return;
	}
	RequestLLMSchedule(_bForce);
}

void ANPCCharacter::OnLearnedInformation(const FString& _info, float _importance, AActor* _source, bool _bReplan)
{
	if (_info.IsEmpty()) return;


	if (FMemoryData* memory = GetMemory())
		memory->AddMemory(_info, _importance, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);


	if (FReflectionData* refl = GetReflection())
		if (refl->Accumulate(_importance) && m_brain)
			m_brain->RequestReflection();


	const bool bDoReplan = _bReplan && (_importance >= ScheduleReplanMinImportance);

	if (bDoReplan)
		ReplanSchedule(/*bForce*/ false);   
}

// ============================================================================
//  NPC-zu-NPC-Gespräche
// ============================================================================
bool ANPCCharacter::CanStartConversation() const
{
	if (!bEnableNPCConversations)          return false;
	if (m_socialCooldown > 0.0f)           return false;
	if (m_conversationPartner.IsValid())   return false;  
	if (m_talkTarget.IsValid())            return false;   

	const FWaypointData* wp = GetComponent<FWaypointData>();
	if (!wp) return false;
	return wp->CurrentGoal == ENPCDesire::Wander || wp->CurrentGoal == ENPCDesire::Idle;
}

ANPCCharacter* ANPCCharacter::FindConversationPartner() const
{
	UWorld* world = GetWorld();
	if (!world) return nullptr;

	const float radiusSq = SocialRadius * SocialRadius;
	for (TActorIterator<ANPCCharacter> it(world); it; ++it)
	{
		ANPCCharacter* other = *it;
		if (!other || other == this) continue;
		if (!other->CanStartConversation()) continue;   


		if (const FRelationshipData* rel = GetComponent<FRelationshipData>())
			if (const FNPCRelation* r = rel->FindRelation(other->GetEntityID()))
				if (r->Liking < AvoidLikingThreshold)
					continue;

		if (FVector::DistSquared(GetActorLocation(), other->GetActorLocation()) <= radiusSq)
			return other;
	}
	return nullptr;
}

bool ANPCCharacter::TryStartNPCConversation()
{
	if (!CanStartConversation()) return false;
	if (ANPCCharacter* partner = FindConversationPartner())
	{
		StartConversationWith(partner);
		return true;
	}
	return false;
}

void ANPCCharacter::TriggerReflection()
{
	if (m_brain)
		m_brain->RequestReflection();   
}

void ANPCCharacter::ExchangeGossip(ANPCCharacter* _other)
{
	if (!_other) return;

	auto shareTop = [](ANPCCharacter* _from, ANPCCharacter* _to)
	{
		if (!_from || !_to) return;
		if (FMemoryData* mem = _from->GetMemory())
		{
			TArray<FMemoryEntry> top = mem->GetTopMemories(1);
			if (top.Num() > 0 && !top[0].Content.IsEmpty())
				_to->OnLearnedInformation(top[0].Content, top[0].Importance * 0.8f, _from, /*bReplan*/ true);
		}
	};
	shareTop(this, _other);
	shareTop(_other, this);
}

void ANPCCharacter::StartConversationWith(ANPCCharacter* _other)
{
	if (!_other || _other == this) return;

	if (m_talkTarget.IsValid() || _other->m_talkTarget.IsValid()) return;

	m_conversationPartner         = _other;
	_other->m_conversationPartner = this;


	const float startTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (FConversationData* conv = GetConversation())          conv->Begin(_other->GetEntityID(), startTime);
	if (FConversationData* conv = _other->GetConversation())  conv->Begin(GetEntityID(), startTime);

	SetTalking(true, _other);
	_other->SetTalking(true, this);

	FString myGossip, theirGossip;
	if (FMemoryData* m = GetMemory())         { auto t = m->GetTopMemories(1); if (t.Num() > 0) myGossip    = t[0].Content; }
	if (FMemoryData* m = _other->GetMemory()) { auto t = m->GetTopMemories(1); if (t.Num() > 0) theirGossip = t[0].Content; }
	ExchangeGossip(_other);

	if (m_brain)
		m_brain->RunConversation(_other, myGossip, theirGossip);


	m_bConvoInitiatorSpeaks = true;
	ConversationGestureTick();
	GetWorldTimerManager().SetTimer(m_conversationGestureTimer, this,
		&ANPCCharacter::ConversationGestureTick, 2.0f, true);


	const float dur = FMath::RandRange(SocialChatDuration * 0.7f, SocialChatDuration * 1.3f);
	GetWorldTimerManager().SetTimer(m_conversationEndTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]() { EndConversation(); }), dur, false);
}

void ANPCCharacter::ConversationGestureTick()
{
	ANPCCharacter* partner = m_conversationPartner.Get();
	if (!partner) { EndConversation(); return; }

	if (m_convLineTexts.IsValidIndex(m_convLineIndex))
	{
		const FString& speakerName = m_convLineSpeakers[m_convLineIndex];
		const FString& text        = m_convLineTexts[m_convLineIndex];

		ANPCCharacter* speaker = nullptr;
		if (speakerName.Equals(GetNPCName(), ESearchCase::IgnoreCase))               speaker = this;
		else if (speakerName.Equals(partner->GetNPCName(), ESearchCase::IgnoreCase)) speaker = partner;
		else speaker = m_bConvoInitiatorSpeaks ? this : partner;

		ANPCCharacter* listener = (speaker == this) ? partner : this;

		speaker->ShowSpeechLine(text);
		speaker->PlayTalkReply();
		listener->HideSpeechBubble();
		listener->EndTalkReply();

		++m_convLineIndex;
		m_bConvoInitiatorSpeaks = (speaker != this);   
		return;
	}


	ANPCCharacter* speaker  = m_bConvoInitiatorSpeaks ? this : partner;
	ANPCCharacter* listener = m_bConvoInitiatorSpeaks ? partner : this;
	if (speaker)  speaker->PlayTalkReply();
	if (listener) listener->EndTalkReply();

	m_bConvoInitiatorSpeaks = !m_bConvoInitiatorSpeaks;
}

void ANPCCharacter::SetConversationSubtitles(const TArray<FString>& _speakers, const TArray<FString>& _texts)
{
	m_convLineSpeakers = _speakers;
	m_convLineTexts    = _texts;
	m_convLineIndex    = 0;
}

void ANPCCharacter::ShowSpeechLine(const FString& _text)
{
	if (!m_speechBubble)
	{
		return;
	}

	UUserWidget* obj = m_speechBubble->GetUserWidgetObject();
	if (!obj)
	{
		return;
	}

	USpeechBubbleWidget* w = Cast<USpeechBubbleWidget>(obj);
	if (!w)
	{
		return;
	}

	w->ShowLine(_text);
	m_speechBubble->SetVisibility(true);
}

void ANPCCharacter::HideSpeechBubble()
{
	if (m_speechBubble)
		m_speechBubble->SetVisibility(false);
}

void ANPCCharacter::EndConversation()
{
	GetWorldTimerManager().ClearTimer(m_conversationEndTimer);
	GetWorldTimerManager().ClearTimer(m_conversationGestureTimer);

	ANPCCharacter* partner = m_conversationPartner.Get();
	m_conversationPartner.Reset();
	m_socialCooldown = SocialCooldown;

	// ECS-Gesprächszustand beenden (Verlauf bleibt für Auswertung stehen).
	if (FConversationData* conv = GetConversation())
		conv->End();

	// Sprechblasen aus, Untertitel leeren (beide Seiten).
	HideSpeechBubble();
	if (partner) partner->HideSpeechBubble();
	m_convLineSpeakers.Reset();
	m_convLineTexts.Reset();
	m_convLineIndex = 0;

	// Eigenen Talk-State verlassen.
	if (m_talkTarget.Get() == partner)
		SetTalking(false);

	// Partner ebenfalls beenden.
	if (partner && partner->m_conversationPartner.Get() == this)
		partner->EndConversation();
}

void ANPCCharacter::RequestLLMSchedule(bool _bForce)
{
	if (m_brain)
		m_brain->RequestSchedule(_bForce);
}

void ANPCCharacter::RequestGreeting()
{
	if (m_brain)
		m_brain->RequestGreeting();
}

FString ANPCCharacter::GenerateHermesPrompt()
{
	return FPromptBuilder::BuildDialoguePrompt(*this);
}

FString ANPCCharacter::GenerateSchedulePrompt()
{
	return FPromptBuilder::BuildSchedulePrompt(*this);
}

FString ANPCCharacter::BuildDialogueContext()
{
	FMemoryData* memory = GetMemory();
	FEmotionData* emotion = GetEmotion();
	FNeedsData* needs = GetNeeds();
	FIdentityData* identity = GetIdentity();
	FRelationshipData* relationships = GetRelationships();

	if (!memory || !emotion || !identity || !relationships)
	{
		return TEXT("Error: Missing componets");
	}

	FString prompt;


	prompt += TEXT("Think and write ONLY in English. Never use any other language, even if ")
		TEXT("the player writes in one - always understand it but reply in English.\n\n");

	// === TEIL 1: IDENTITÄT ===
	prompt += FString::Printf(TEXT("You are %s, a %s. \n"), *identity->Name, *identity->Role);
	if (!identity->Personality.IsEmpty())
	{
		prompt += FString::Printf(TEXT("Your personality: %s \n"), *identity->Personality);
	}
	if (identity->Goals.Num() > 0)
	{
		prompt += TEXT("Your goals: \n");
		for (const FString& goal : identity->Goals)
		{
			prompt += FString::Printf(TEXT("- %s\n"), *goal);
		}
	}
	prompt += TEXT("\n");

	// === TEIL 2: MEMORIES ===
	TArray<FMemoryEntry> topMemories = memory->GetTopMemories(5);
	if (topMemories.Num() > 0)
	{
		prompt += TEXT("Recent memories: \n");
		for (const FMemoryEntry& mem : topMemories)
		{
			prompt += FString::Printf(TEXT("- %s (Importance: %.1f%%)\n"),
			                          *mem.Content, mem.Importance * 100.0f);
		}
		prompt += TEXT("\n");
	}

	// === TEIL 3: EMOTION ===
	EEmotionType dominantEmotion = emotion->GetDominantEmotion();
	float emotionIntensity = emotion->GetDominantIntensity();

	if (emotionIntensity > 0.1f)
	{
		switch (dominantEmotion)
		{
		case EEmotionType::Happy:
			prompt += FString::Printf(TEXT("You feel happy (%.0f%%).\n"), emotionIntensity * 100.0f);
			break;
		case EEmotionType::Suspicious:
			prompt += FString::Printf(TEXT("You feel suspicious (%.0f%%).\n"), emotionIntensity * 100.0f);
			break;
		case EEmotionType::Concerned:
			prompt += FString::Printf(TEXT("You feel concerned (%.0f%%).\n"), emotionIntensity * 100.0f);
			break;
		case EEmotionType::Angry:
			prompt += FString::Printf(TEXT("You feel angry (%.0f%%).\n"), emotionIntensity * 100.0f);
			break;
		default:
			prompt += TEXT("You feel neutral.\n");
			break;
		}
	}

	// == TEIL 4: NEEDS ==
	if (needs->IsHungry())
	{
		prompt += FString::Printf(TEXT("You are very hungry (%.0f%).\n"), needs->Hunger * 100.0f);
	}
	if (needs->IsExhausted())
	{
		prompt += FString::Printf(TEXT("You are very exhausted (%.0f%)"), needs->Exhaustion * 100.0f);
	}
	if (!needs->IsHungry() && !needs->IsExhausted())
	{
		prompt += TEXT("You feel well.\n");
	}
	prompt += TEXT("\n");

	// === TEIL 5: RELATIONSHIPS ===
	if (relationships && relationships->Relations.Num() > 0)
	{
		prompt += TEXT("People you know:\n");
		for (const FNPCRelation& Rel : relationships->Relations)
		{
			FString Sentiment;
			if (Rel.Liking >= 0.75f && Rel.Trust >= 0.65f) Sentiment = TEXT("are good friends with");
			else if (Rel.Liking > 0.55f)                   Sentiment = TEXT("like");
			else if (Rel.Liking < 0.45f)                   Sentiment = TEXT("dislike");
			else                                           Sentiment = TEXT("are neutral about");

			const FString name = Rel.OtherName.IsEmpty()
				? FString::Printf(TEXT("someone (#%d)"), Rel.OtherEntityID)
				: Rel.OtherName;
			prompt += FString::Printf(TEXT("- You %s %s (liking %.0f%%, trust %.0f%%)\n"),
			                          *Sentiment, *name, Rel.Liking * 100.0f, Rel.Trust * 100.0f);

			if (!Rel.Opinion.IsEmpty())
				prompt += FString::Printf(TEXT("    (your view: %s)\n"), *Rel.Opinion);
			else
				for (const FString& mText : Rel.Memories)
					prompt += FString::Printf(TEXT("    · %s\n"), *mText);
		}
		prompt += TEXT("\n");
	}

	// === TEIL 5b: LETZTES NPC-GESPRÄCH ===
	if (const FConversationData* conv = GetComponent<FConversationData>())
	{
		if (conv->ConversationHistory.Num() > 0)
		{
			prompt += TEXT("Your most recent chat with another villager:\n");
			for (const FString& line : conv->ConversationHistory)
				prompt += FString::Printf(TEXT("- %s\n"), *line);
			prompt += TEXT("\n");
		}
	}

	// === TEIL 5c: OFFENE QUESTS DES SPIELERS BEI DIESEM NPC ===
	if (const UWorld* w = GetWorld())
		if (const APlayerController* pc = w->GetFirstPlayerController())
			if (const ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(pc->GetPawn()))
			{
				const FString myName = GetNPCName();
				FString questLines;
				for (const FQuestData& q : player->GetActiveQuests())
				{
					if (q.GiverName != myName) continue;
					if (q.Type == EQuestType::Delivery)
						questLines += FString::Printf(
							TEXT("- \"%s\": you asked the player to carry a message to %s (reward %d gold). Message: %s\n"),
							*q.Title, *q.TargetNpc, q.RewardGold,
							q.Description.IsEmpty() ? TEXT("-") : *q.Description);
					else
						questLines += FString::Printf(
							TEXT("- \"%s\": the player still owes you %d x %s (reward %d gold). Why you asked: %s\n"),
							*q.Title, q.TargetCount, *q.TargetItem.ToString(), q.RewardGold,
							q.Description.IsEmpty() ? TEXT("-") : *q.Description);
				}
				if (!questLines.IsEmpty())
				{
					prompt += TEXT("Quests YOU already gave the player (refer to them; ask about progress or thank them on delivery; do NOT give a new quest for the same thing):\n");
					prompt += questLines;
					prompt += TEXT("\n");
				}
			}

	// === TEIL 6: ACTION ===
	FString intentOptions;
	TArray<EIntentType> intents = {
		EIntentType::Neutral, EIntentType::Greeting,
		EIntentType::Trade, EIntentType::Hostile, EIntentType::Quest
	};
	for (int32 i = 0; i < intents.Num(); i++)
	{
		intentOptions += IntentToString(intents[i]);
		if (i < intents.Num() - 1) intentOptions += TEXT("|");
	}

	// Mood-Optionen aus Enum aufbauen
	FString moodOptions;
	TArray<EEmotionType> moods = {
		EEmotionType::Neutral, EEmotionType::Happy,
		EEmotionType::Suspicious, EEmotionType::Angry
	};
	for (int32 i = 0; i < moods.Num(); i++)
	{
		moodOptions += EmotionToMoodString(moods[i]);
		if (i < moods.Num() - 1) moodOptions += TEXT("|");
	}

	// Quest-Grounding: welche Items existieren
	FString itemList;
	if (const UWorld* w = GetWorld())
		if (const ALLM_NPCGameMode* gm = w->GetAuthGameMode<ALLM_NPCGameMode>())
			itemList = gm->GetItemListString();


	FString npcList;
	FString friendList;
	if (const UWorld* w = GetWorld())
	{
		const FString myName = GetNPCName();
		const FRelationshipData* rel = GetComponent<FRelationshipData>();
		for (TActorIterator<ANPCCharacter> it(w); it; ++it)
		{
			const ANPCCharacter* other = *it;
			if (!other || other == this) continue;
			const FString on = other->GetNPCName();
			if (on.IsEmpty() || on.Equals(myName)) continue;
			if (!npcList.IsEmpty()) npcList += TEXT(", ");
			npcList += on;

			// Gute Kontakte vormerken (Standard 0.5 = neutral, ≥ 0.6 = mag ich).
			float liking = 0.5f;
			if (rel)
				if (const FNPCRelation* r = rel->FindRelation(other->GetEntityID()))
					liking = r->Liking;
			if (liking >= 0.6f)
			{
				if (!friendList.IsEmpty()) friendList += TEXT(", ");
				friendList += on;
			}
		}
	}

	prompt += TEXT("RULES:\n");
	prompt += TEXT("- Maximum 1 sentence in \"reply\". Never more.\n");
	prompt += TEXT("- Never write the player's next line.\n");
	prompt += TEXT("- Never mention being an AI.\n");
	prompt += TEXT("- LANGUAGE: reply ONLY in English. Never switch to another language.\n");

	if (!itemList.IsEmpty())
	{
		const bool bCanDeliver = !npcList.IsEmpty();


		prompt += FString::Printf(
			TEXT("- QUESTS: ONLY if the player's message contains one of the words work, job, task, ")
			TEXT("quest, help, hire, message, deliver, errand, letter, tell or send, set intent \"quest\" ")
			TEXT("AND add a \"quest\" object; otherwise NEVER give a quest. Words like message, deliver, ")
			TEXT("errand, letter, tell or send point to a \"delivery\" quest. Set \"kind\" to one of these types:\n")
			TEXT("    \"fetch\": the player brings items back to YOU (this character) - no one else. ")
			TEXT("\"item\" is EXACTLY one of [%s]; \"count\" 1-5; \"description\" is ONE short sentence why YOU need them.\n"),
			*itemList);

		if (bCanDeliver)
		{
			const FString pref = friendList.IsEmpty()
				? FString()
				: FString::Printf(TEXT(" - prefer someone you are on good terms with, such as %s"), *friendList);
			prompt += FString::Printf(
				TEXT("    \"delivery\": the player carries a short spoken message to ANOTHER villager you TRUST. ")
				TEXT("\"target\" is EXACTLY one of [%s] (never yourself)%s. ")
				TEXT("Put the message you want passed on into \"description\".\n"),
				*npcList, *pref);
		}

		prompt += TEXT("  Always set a SHORT \"title\" (a label of at most 4 words - NOT the message or a sentence) and \"reward\" (10-50 gold). Omit the \"quest\" field entirely if not giving a quest.\n");

		prompt += TEXT("- Respond ONLY with JSON, nothing else. Normal reply:\n\n");
		prompt += TEXT("{\n");
		prompt += TEXT("  \"reply\": \"your single sentence here\",\n");
		prompt += FString::Printf(TEXT("  \"intent\": \"%s\",\n"), *intentOptions);
		prompt += FString::Printf(TEXT("  \"mood\": \"%s\"\n"), *moodOptions);
		prompt += TEXT("}\n\n");

		prompt += TEXT("A fetch quest looks like this:\n\n");
		prompt += TEXT("{\n");
		prompt += TEXT("  \"reply\": \"one sentence offering the task\",\n");
		prompt += TEXT("  \"intent\": \"quest\",\n");
		prompt += FString::Printf(TEXT("  \"mood\": \"%s\",\n"), *moodOptions);
		prompt += TEXT("  \"quest\": {\"kind\": \"fetch\", \"title\": \"short title\", \"description\": \"why you need these items\", \"item\": \"one of the allowed items\", \"count\": 2, \"reward\": 20}\n");
		prompt += TEXT("}");

		if (bCanDeliver)
		{
			prompt += TEXT("\n\nA delivery quest looks like this:\n\n");
			prompt += TEXT("{\n");
			prompt += TEXT("  \"reply\": \"one sentence asking the player to carry your message\",\n");
			prompt += TEXT("  \"intent\": \"quest\",\n");
			prompt += FString::Printf(TEXT("  \"mood\": \"%s\",\n"), *moodOptions);
			prompt += TEXT("  \"quest\": {\"kind\": \"delivery\", \"title\": \"short title\", \"description\": \"the exact message to pass on\", \"target\": \"one of the villager names\", \"reward\": 15}\n");
			prompt += TEXT("}");
		}
	}
	else
	{
		prompt += TEXT("- Respond ONLY with this JSON, nothing else:\n\n");
		prompt += TEXT("{\n");
		prompt += TEXT("  \"reply\": \"your single sentence here\",\n");
		prompt += FString::Printf(TEXT("  \"intent\": \"%s\",\n"), *intentOptions);
		prompt += FString::Printf(TEXT("  \"mood\": \"%s\"\n"), *moodOptions);
		prompt += TEXT("}");
	}

	return prompt;
}

FString ANPCCharacter::BuildScheduleContext()
{
	if (!NPCData) return TEXT("");

	FString prompt;
	prompt += FString::Printf(
		TEXT("You are planning the daily schedule for %s, a %s.\n"),
		*NPCData->NPCName, *NPCData->GetOccupationString());

	// Wochentag mitgeben → der Plan darf sich je Tag unterscheiden (Markttag,
	// Ruhetag, Freunde besuchen …).
	if (const UWorld* w = GetWorld())
		if (const UWorldClock* clock = w->GetSubsystem<UWorldClock>())
			prompt += FString::Printf(
				TEXT("Today is %s. Vary your routine by weekday where it makes sense (e.g. a busier work/market day, a quieter rest day, visiting friends).\n"),
				*clock->GetWeekdayName());

	if (!NPCData->RoutineDescription.IsEmpty())
		prompt += FString::Printf(TEXT("Character routine: %s\n"), *NPCData->RoutineDescription);

	// Erlaubte Aktionen
	prompt += TEXT("\nUse ONLY these actions, nothing else: work, eat, drink, rest, sleep, wander, shop. Do NOT use 'wake' or any other word.\n");
	prompt += TEXT("Use 'shop' when this character would buy goods at the market (only on days the market is open).\n");
	if (NPCData)
	{
		prompt += FString::Printf(
			TEXT("As a %s you usually sleep around %d:00 and wake around %d:00 – but YOU decide your own sleep and wake times.\n"),
			*NPCData->GetOccupationString(),
			SleepHourForType(NPCData->SleepType),
			WakeHourForType(NPCData->SleepType));
	}
	prompt += TEXT("Plan a realistic full day. Include exactly ONE 'sleep' entry and put it at NIGHT (evening/night hours); NEVER sleep during the day. Do not add a wake action - you simply resume activities after sleeping. During the day use rest if you want a short break.\n");

	// Was der NPC kürzlich erfahren hat (Spieler-Gespräch, andere NPCs) 
	if (FMemoryData* memory = GetMemory())
	{
		TArray<FMemoryEntry> recent = memory->GetTopMemories(3);
		if (recent.Num() > 0)
		{
			prompt += TEXT("\nRecent things you learned (take these into account):\n");
			for (const FMemoryEntry& m : recent)
				prompt += FString::Printf(TEXT("- %s\n"), *m.Content);
		}
	}

	prompt += TEXT("\nUse AT MOST 8 entries, sorted by hour, compact. ");
	prompt += TEXT("Respond with ONLY this JSON on a single line, nothing else:\n");
	prompt += TEXT("{\"schedule\":[{\"hour\":6,\"action\":\"work\"},{\"hour\":12,\"action\":\"eat\"},{\"hour\":18,\"action\":\"drink\"},{\"hour\":22,\"action\":\"sleep\"}]}");

	return prompt;
}

FString ANPCCharacter::GenerateMemoryPrompt()
{
	FMemoryData* memory = GetMemory();
	if (!memory || memory->Memories.Num() == 0)
	{
		return TEXT("You haven't experienced anything yet.");
	}

	FString prompt = TEXT("Your memories:\n");
	TArray<FMemoryEntry> topMemories = memory->GetTopMemories(5);

	for (int32 i = 0; i < topMemories.Num(); ++i)
	{
		prompt += FString::Printf(TEXT("%d. %s (Importance: %.1f%%)\n"),
		                          i + 1, *topMemories[i].Content, topMemories[i].Importance * 100.0f);
	}

	return prompt;
}


FString ANPCCharacter::GenerateNeedsPrompt()
{
	FNeedsData* needs = GetNeeds();
	if (!needs)
		return TEXT("");
	FString prompt = TEXT("Your physical state:\n");

	if (needs->IsHungry())
		prompt += FString::Printf(TEXT("- Hunger: %.0f%% (CRITICAL)\n"), needs->Hunger * 100.0f);
	else
		prompt += FString::Printf(TEXT("- Hunger: %.0f%%\n"), needs->Hunger * 100.0f);

	if (needs->IsExhausted())
		prompt += FString::Printf(TEXT("- Exhaustion: %.0f%% (CRITICAL)\n"), needs->Exhaustion * 100.0f);
	else
		prompt += FString::Printf(TEXT("- Exhaustion: %.0f%%\n"), needs->Exhaustion * 100.0f);

	return prompt;
}

FString ANPCCharacter::GenerateEmotionPrompt()
{
	FEmotionData* emotion = GetEmotion();
	if (!emotion)
		return TEXT("");

	FString prompt;
	EEmotionType dominant = emotion->GetDominantEmotion();
	float Intensity = emotion->GetDominantIntensity();

	if (Intensity < 0.1f)
	{
		prompt = TEXT("You feel emotionally neutral.");
	}
	else
	{
		switch (dominant)
		{
		case EEmotionType::Happy:
			prompt = FString::Printf(TEXT("You are feeling quite happy (%.0f%% intensity)."), Intensity * 100.0f);
			break;
		case EEmotionType::Suspicious:
			prompt = FString::Printf(TEXT("You feel suspicious and wary (%.0f%% intensity)."), Intensity * 100.0f);
			break;
		case EEmotionType::Concerned:
			prompt = FString::Printf(TEXT("You are feeling concerned (%.0f%% intensity)."), Intensity * 100.0f);
			break;
		case EEmotionType::Angry:
			prompt = FString::Printf(TEXT("You are angry (%.0f%% intensity)."), Intensity * 100.0f);
			break;
		default:
			prompt = TEXT("You feel neutral.");
			break;
		}
	}
	return prompt;
}

void ANPCCharacter::ApplyDialogueResult(const FString& _reply, EIntentType _intent, EEmotionType _mood)
{
	if (m_brain)
		m_brain->ApplyDialogueResult(_reply, _intent, _mood);
}

void ANPCCharacter::AddDialogTurn(const FString& _text, bool _bIsNPCResponse)
{
	FDialogData* dialog = GetDialog();
	if (!dialog) return;

	FDialogTurn turn;
	turn.bIsNPCResponse = _bIsNPCResponse;
	turn.DialogText     = _text;
	turn.Timestamp      = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	dialog->DialogHistory.Add(turn);


	while (dialog->DialogHistory.Num() > FDialogData::MAX_DIALOG_HISTORY)
		dialog->DialogHistory.RemoveAt(0);
}

FNPCDialogResponse ANPCCharacter::ParseLLMResponse(const FString& RawResponse)
{

	return m_brain ? m_brain->ParseLLMResponse(RawResponse) : FNPCDialogResponse{};
}
