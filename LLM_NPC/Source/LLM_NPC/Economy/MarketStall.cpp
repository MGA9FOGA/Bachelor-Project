#include "MarketStall.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ArrowComponent.h"
#include "ItemDataAsset.h"
#include "LLM_NPC/World/WorldClock.h"
#include "LLM_NPC/NPC/NPCCharacter.h"
#include "LLM_NPC/LLM_NPCCharacter.h"
#include "LLM_NPC/UI/ShopWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

AMarketStall::AMarketStall()
{
	PrimaryActorTick.bCanEverTick = false;

	m_root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(m_root);

	m_mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	m_mesh->SetupAttachment(m_root);

	m_interactSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	m_interactSphere->SetupAttachment(m_root);
	m_interactSphere->SetSphereRadius(220.0f);
	m_interactSphere->SetCollisionProfileName(TEXT("Trigger"));

	// Stand-Slots: der Pfeil selbst ist der Slot. Seine +X-Richtung ist die
	// Blickrichtung des NPCs – im Viewport drehen setzt also direkt den Blick.
	// Verkäufer: hinter der Theke (−X), Blick nach vorn (+X) zur Kundenseite.
	VendorSlot = CreateDefaultSubobject<UArrowComponent>(TEXT("VendorSlot"));
	VendorSlot->SetupAttachment(m_root);
	VendorSlot->SetRelativeLocation(FVector(-90.0f, 0.0f, 0.0f));
	VendorSlot->SetHiddenInGame(true);

	// Käufer: vor der Theke (+X), Blick nach hinten (−X) zum Stand → Yaw 180°.
	CustomerSlot = CreateDefaultSubobject<UArrowComponent>(TEXT("CustomerSlot"));
	CustomerSlot->SetupAttachment(m_root);
	CustomerSlot->SetRelativeLocation(FVector(120.0f, 0.0f, 0.0f));
	CustomerSlot->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	CustomerSlot->SetHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	// Kräftig sichtbar, damit die Blickrichtung im Editor eindeutig ist.
	VendorSlot->ArrowSize     = 2.0f;
	VendorSlot->ArrowLength   = 80.0f;
	VendorSlot->ArrowColor    = FColor(60, 180, 255);   // blau = Verkäufer
	VendorSlot->bIsScreenSizeScaled = true;

	CustomerSlot->ArrowSize   = 2.0f;
	CustomerSlot->ArrowLength = 80.0f;
	CustomerSlot->ArrowColor  = FColor(255, 180, 60);   // orange = Käufer
	CustomerSlot->bIsScreenSizeScaled = true;
#endif
}

FTransform AMarketStall::GetVendorStandTransform() const
{
	return VendorSlot ? VendorSlot->GetComponentTransform() : GetActorTransform();
}

FTransform AMarketStall::GetCustomerStandTransform() const
{
	return CustomerSlot ? CustomerSlot->GetComponentTransform() : GetActorTransform();
}

bool AMarketStall::TryReserveCustomer(uint32 _entity)
{
	if (m_customerOccupant == 0 || m_customerOccupant == _entity)
	{
		m_customerOccupant = _entity;
		return true;
	}
	return false;   
}

void AMarketStall::ReleaseCustomer(uint32 _entity)
{
	if (m_customerOccupant == _entity)
		m_customerOccupant = 0;
}

void AMarketStall::BeginPlay()
{
	Super::BeginPlay();
	m_interactSphere->OnComponentBeginOverlap.AddDynamic(this, &AMarketStall::OnSphereBegin);
	m_interactSphere->OnComponentEndOverlap.AddDynamic(this, &AMarketStall::OnSphereEnd);

	if (Vendor)
		Vendor->SetAssignedStall(this);
}

// ============================================================================
//  Öffnungszeiten (nutzt den 7-Tage-Rhythmus der WorldClock)
// ============================================================================
bool AMarketStall::IsOpenToday() const
{
	const UWorld* w = GetWorld();
	const UWorldClock* clock = w ? w->GetSubsystem<UWorldClock>() : nullptr;
	if (!clock) return false;
	return (OpeningDays & (1 << clock->GetWeekday())) != 0;
}

bool AMarketStall::IsOpenNow() const
{
	if (!IsOpenToday()) return false;
	const UWorld* w = GetWorld();
	const UWorldClock* clock = w ? w->GetSubsystem<UWorldClock>() : nullptr;
	if (!clock) return false;
	const int32 h = clock->GetHour();
	return h >= OpenHour && h < CloseHour;
}

bool AMarketStall::IsVendorPresent() const
{
	const ANPCCharacter* v = Vendor.Get();
	if (!v) return false;
	if (!VendorSlot) return true;   

	const float distSq = FVector::DistSquared(
		v->GetActorLocation(), VendorSlot->GetComponentLocation());
	return distSq <= FMath::Square(VendorPresentRadius);
}

int32 AMarketStall::GetPrice(int32 _goodIndex) const
{
	if (!Goods.IsValidIndex(_goodIndex) || !Goods[_goodIndex].Item) return -1;
	const FMarketGood& g = Goods[_goodIndex];
	return (g.PriceOverride > 0) ? g.PriceOverride : g.Item->BasePrice;
}

int32 AMarketStall::GetAdjustedPrice(int32 _goodIndex, float _buyerLiking) const
{
	const int32 base = GetPrice(_goodIndex);
	if (base < 0) return -1;

	// Sympathie um Neutral (0.5) zentrieren: +0.5 = Freund → Rabatt, −0.5 = Abneigung → Aufschlag.
	const float centered = FMath::Clamp(_buyerLiking, 0.0f, 1.0f) - 0.5f;
	const float factor   = 1.0f - centered * 2.0f * PriceSensitivity;   // [1-Sens .. 1+Sens]
	return FMath::Max(1, FMath::RoundToInt(base * factor));             // mind. 1 Gold
}

// ============================================================================
//  Kauf
// ============================================================================
bool AMarketStall::TryBuy(ALLM_NPCCharacter* _buyer, int32 _goodIndex)
{
	if (!_buyer)             return false;
	if (!IsOpenNow())        return false;
	if (!Goods.IsValidIndex(_goodIndex) || !Goods[_goodIndex].Item) return false;

	FMarketGood& g = Goods[_goodIndex];
	if (g.Stock == 0) return false;   // ausverkauft (-1 = unbegrenzt)

	// Preis nach Sympathie des Verkäufers zum Spieler.
	const float liking = Vendor ? Vendor->GetPlayerLiking() : 0.5f;
	const int32 price  = GetAdjustedPrice(_goodIndex, liking);
	if (price < 0) return false;

	if (!_buyer->SpendGold(price)) return false;   

	_buyer->AddItem(g.Item->ItemId, 1);
	if (g.Stock > 0) g.Stock--;
	if (Vendor) Vendor->AddGold(price);

	return true;
}

bool AMarketStall::TryBuyForNPC(ANPCCharacter* _buyer, int32 _goodIndex)
{
	if (!_buyer)          return false;
	if (_buyer == Vendor) return false;   // niemand kauft im EIGENEN Stand
	if (!IsOpenNow())     return false;
	if (!Goods.IsValidIndex(_goodIndex) || !Goods[_goodIndex].Item) return false;

	FMarketGood& g = Goods[_goodIndex];
	if (g.Stock == 0) return false;   // ausverkauft (-1 = unbegrenzt)

	// Preis nach Sympathie des Verkäufers zum kaufenden NPC.
	const float liking = (Vendor && Vendor != _buyer)
		? Vendor->GetLikingToward(_buyer->GetEntityID()) : 0.5f;
	const int32 price  = GetAdjustedPrice(_goodIndex, liking);
	if (price < 0) return false;

	if (!_buyer->SpendGold(price)) return false;   // nicht genug Gold

	_buyer->AddItem(g.Item->ItemId, 1);
	if (g.Stock > 0) g.Stock--;
	if (Vendor && Vendor != _buyer) Vendor->AddGold(price);

	return true;
}

// ============================================================================
//  Interaktion – öffnet den „Shop" (aktuell: Angebot ins Log).
//  Ein UMG-Shop-Widget kann später TryBuy(index) aufrufen.
// ============================================================================
TArray<FInventorySlot> AMarketStall::GetOfferSlots() const
{
	TArray<FInventorySlot> slots;
	slots.Reserve(Goods.Num());

	// Angebot fürs Shop-UI = für den Spieler → Sympathie des Verkäufers zum Spieler.
	const float liking = Vendor ? Vendor->GetPlayerLiking() : 0.5f;

	// WICHTIG: keine Einträge überspringen – der Index muss zu Goods passen,
	// damit er direkt an TryBuy weitergereicht werden kann.
	for (int32 i = 0; i < Goods.Num(); ++i)
	{
		FInventorySlot s;
		if (const UItemDataAsset* item = Goods[i].Item)
		{
			s.ItemId      = item->ItemId;
			s.DisplayName = item->DisplayName;
			s.Icon        = item->Icon;
			s.Price       = GetAdjustedPrice(i, liking);
			s.Count       = Goods[i].Stock;   // -1 = unbegrenzt
		}
		slots.Add(s);
	}
	return slots;
}

void AMarketStall::OnInteract(AActor* _interactor)
{
	ALLM_NPCCharacter* buyer = Cast<ALLM_NPCCharacter>(_interactor);

	if (!IsOpenNow())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Markt] %s ist gerade GESCHLOSSEN (offen: %s, %02d–%02d Uhr)"),
			   *StallName, *OpeningDaysText(), OpenHour, CloseHour);
		return;
	}

	// Schon ein Shop-Fenster offen? → kein zweites aufmachen
	if (m_openShop && m_openShop->IsInViewport())
		return;

	// Shop-UI öffnen (falls ein WBP zugewiesen ist)
	if (buyer && ShopWidgetClass)
	{
		if (APlayerController* pc = Cast<APlayerController>(buyer->GetController()))
		{
			if (UShopWidget* shop = CreateWidget<UShopWidget>(pc, ShopWidgetClass))
			{
				m_openShop = shop;
				shop->Setup(this, buyer);
				shop->AddToViewport();

				FInputModeGameAndUI mode;
				mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				mode.SetHideCursorDuringCapture(false);   // Cursor beim UI-Drag NICHT verstecken
				pc->SetInputMode(mode);
				pc->bShowMouseCursor = true;
				return;
			}
		}
	}


	for (int32 i = 0; i < Goods.Num(); ++i)
	{
		if (!Goods[i].Item) continue;
		const FString stock = Goods[i].Stock < 0 ? TEXT("unbegrenzt") : FString::FromInt(Goods[i].Stock);

	}

}

FString AMarketStall::GetInteractionPrompt()
{
	if (IsOpenNow())
		return FString::Printf(TEXT("%s (open) - interact to shop"), *StallName);
	return FString::Printf(TEXT("%s (closed - open: %s)"), *StallName, *OpeningDaysText());
}

// ============================================================================
//  Reichweiten-Sphere → beim Spieler als Interactable an-/abmelden
// ============================================================================
void AMarketStall::OnSphereBegin(UPrimitiveComponent*, AActor* _other, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(_other))
		player->AddNearbyInteractable(this);
}

void AMarketStall::OnSphereEnd(UPrimitiveComponent*, AActor* _other, UPrimitiveComponent*, int32)
{
	if (ALLM_NPCCharacter* player = Cast<ALLM_NPCCharacter>(_other))
		player->RemoveNearbyInteractable(this);
}

FString AMarketStall::OpeningDaysText() const
{
	static const TCHAR* Short[7] = { TEXT("Mon"), TEXT("Tue"), TEXT("Wed"), TEXT("Thu"),
		TEXT("Fri"), TEXT("Sat"), TEXT("Sun") };
	TArray<FString> days;
	for (int32 i = 0; i < 7; ++i)
		if (OpeningDays & (1 << i)) days.Add(Short[i]);
	return days.Num() > 0 ? FString::Join(days, TEXT(", ")) : TEXT("never");
}
