// Copyright Epic Games, Inc. All Rights Reserved.

#include "LLM_NPCGameMode.h"
#include "LLM_NPCCharacter.h"
#include "Economy/ItemDataAsset.h"
#include "Economy/MarketStall.h"
#include "UI/ShopWidget.h"
#include "NPC/NPCCharacter.h"
#include "World/WorldSpot.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ALLM_NPCGameMode::ALLM_NPCGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_LLM_NPCCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

FString ALLM_NPCGameMode::GetItemListString() const
{
	TArray<FString> ids;
	for (const TObjectPtr<UItemDataAsset>& item : KnownItems)
		if (item && !item->ItemId.IsNone())
			ids.Add(item->ItemId.ToString());
	return FString::Join(ids, TEXT(", "));
}

UItemDataAsset* ALLM_NPCGameMode::FindItem(FName _itemId) const
{
	for (const TObjectPtr<UItemDataAsset>& item : KnownItems)
		if (item && item->ItemId == _itemId)
			return item;
	return nullptr;
}

TArray<FInventorySlot> ALLM_NPCGameMode::MakeSlots(const FInventoryData& _inv) const
{
	TArray<FInventorySlot> slots;
	for (const TPair<FName, int32>& pair : _inv.Items)
	{
		FInventorySlot slot;
		slot.ItemId = pair.Key;
		slot.Count  = pair.Value;
		if (const UItemDataAsset* def = FindItem(pair.Key))
		{
			slot.DisplayName = def->DisplayName;
			slot.Price       = def->BasePrice;
			slot.Icon        = def->Icon;
		}
		else
		{
			slot.DisplayName = pair.Key.ToString();   // Fallback: die ID
		}
		slots.Add(slot);
	}
	return slots;
}

// ============================================================================
//  Startup-Selbstcheck: schreibt die Markt-/Wirtschafts-Konfiguration ins Log.
// ============================================================================
void ALLM_NPCGameMode::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(m_selfCheckTimer, this,
		&ALLM_NPCGameMode::LogMarketSelfCheck, 2.0f, false);
}

void ALLM_NPCGameMode::LogMarketSelfCheck() const
{
	UWorld* w = GetWorld();
	if (!w) return;

	UE_LOG(LogTemp, Warning, TEXT("======== [MarktCheck] START ========"));

	// --- 1) Items ---
	UE_LOG(LogTemp, Warning, TEXT("[MarktCheck] KnownItems: %d"), KnownItems.Num());
	if (KnownItems.Num() == 0)
		UE_LOG(LogTemp, Error, TEXT("[MarktCheck] KEINE KnownItems! → keine Namen/Preise/Icons, kein Konsum/Handel."));
	for (const TObjectPtr<UItemDataAsset>& it : KnownItems)
	{
		if (!it) { UE_LOG(LogTemp, Error, TEXT("[MarktCheck]   <LEERER Eintrag in KnownItems!>")); continue; }
		if (it->bConsumable)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MarktCheck]   %s: %d Gold | KONSUM %s %.2f"),
		*it->ItemId.ToString(), it->BasePrice,
		it->ConsumeType == EConsumeType::Food ? TEXT("Food") : TEXT("Drink"), it->Nourishment);	
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MarktCheck]   %s: %d Gold"), *it->ItemId.ToString(), it->BasePrice);	
		}
	}

	// --- 2) Marktstände ---
	int32 stalls = 0;
	for (TActorIterator<AMarketStall> it(w); it; ++it)
	{
		AMarketStall* s = *it;
		if (!s) continue;
		++stalls;
		UE_LOG(LogTemp, Warning,
			TEXT("[MarktCheck] Stand '%s': Vendor=%s, Goods=%d, offen_jetzt=%s, VendorSlot=%s, CustomerSlot=%s, ShopWidget=%s"),
			*s->StallName,
			s->Vendor ? *s->Vendor->GetNPCName() : TEXT("KEINER!"),
			s->Goods.Num(),
			s->IsOpenNow() ? TEXT("ja") : TEXT("nein"),
			s->HasVendorStand()   ? TEXT("ok") : TEXT("FEHLT"),
			s->HasCustomerStand() ? TEXT("ok") : TEXT("FEHLT"),
			s->ShopWidgetClass    ? TEXT("ok") : TEXT("FEHLT"));
	}
	if (stalls == 0)
		UE_LOG(LogTemp, Error, TEXT("[MarktCheck] KEINE Marktstände im Level!"));

	// --- 3) Spots zählen ---
	int32 rest = 0, eat = 0, work = 0, sleep = 0, shop = 0;
	for (TActorIterator<AWorldSpot> it(w); it; ++it)
	{
		AWorldSpot* sp = *it;
		if (!sp) continue;
		switch (sp->Type)
		{
			case ESpotType::Rest:  ++rest;  break;
			case ESpotType::Eat:   ++eat;   break;
			case ESpotType::Work:  ++work;  break;
			case ESpotType::Sleep: ++sleep; break;
			case ESpotType::Shop:  ++shop;  break;
			default: break;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[MarktCheck] Spots: Rest=%d Eat=%d Work=%d Sleep=%d Shop=%d"),
		rest, eat, work, sleep, shop);
	if (rest == 0)
		UE_LOG(LogTemp, Warning, TEXT("[MarktCheck] Keine Rest-Spots → Konsum am Rest-Platz fällt auf die Taverne zurück."));

	// --- 4) NPCs ---
	int32 npcs = 0;
	for (TActorIterator<ANPCCharacter> it(w); it; ++it)
	{
		ANPCCharacter* npc = *it;
		if (!npc) continue;
		++npcs;
		UE_LOG(LogTemp, Warning, TEXT("[MarktCheck] NPC '%s': Gold=%d, Rolle=%s"),
			*npc->GetNPCName(), npc->GetGold(),
			npc->GetAssignedStall() ? TEXT("Verkaeufer") : TEXT("-"));
	}
	UE_LOG(LogTemp, Warning, TEXT("[MarktCheck] NPCs gesamt: %d"), npcs);

	UE_LOG(LogTemp, Warning, TEXT("======== [MarktCheck] ENDE ========"));
}
