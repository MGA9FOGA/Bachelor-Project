#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LLM_NPC/Interface/InteractableInterface.h"
#include "LLM_NPC/Economy/InventorySlot.h"
#include "MarketStall.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class USceneComponent;
class UArrowComponent;
class UItemDataAsset;
class ANPCCharacter;
class ALLM_NPCCharacter;
class UShopWidget;

// Öffnungstage als Bitmask – Reihenfolge passend zu UWorldClock::GetWeekday() (0 = Montag).
UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EWeekdayFlags : uint8
{
	None      = 0        UMETA(Hidden),
	Monday    = 1 << 0   UMETA(DisplayName = "Montag"),
	Tuesday   = 1 << 1   UMETA(DisplayName = "Dienstag"),
	Wednesday = 1 << 2   UMETA(DisplayName = "Mittwoch"),
	Thursday  = 1 << 3   UMETA(DisplayName = "Donnerstag"),
	Friday    = 1 << 4   UMETA(DisplayName = "Freitag"),
	Saturday  = 1 << 5   UMETA(DisplayName = "Samstag"),
	Sunday    = 1 << 6   UMETA(DisplayName = "Sonntag")
};

// Eine angebotene Ware an einem Stand.
USTRUCT(BlueprintType)
struct FMarketGood
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Market")
	TObjectPtr<UItemDataAsset> Item;

	// 0 = BasePrice des Items nutzen; sonst dieser Preis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Market", meta=(ClampMin=0))
	int32 PriceOverride = 0;

	// -1 = unbegrenzt, sonst verfügbare Stückzahl.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Market", meta=(ClampMin=-1))
	int32 Stock = -1;
};

/**
 * AMarketStall – ein Marktstand mit Öffnungstagen, Warenangebot und (optional) einem
 * Händler-NPC. Der Spieler interagiert per Interact-Taste → Kauf (Gold gegen Ware).
 * Öffnungstage nutzen den 7-Tage-Rhythmus der WorldClock.
 */
UCLASS()
class LLM_NPC_API AMarketStall : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AMarketStall();

	// === Konfiguration ===
	UPROPERTY(EditAnywhere, Category="Market")
	FString StallName = TEXT("Marktstand");

	// An welchen Wochentagen der Stand offen hat (Bitmask, mehrere ankreuzbar).
	UPROPERTY(EditAnywhere, Category="Market", meta=(Bitmask, BitmaskEnum="/Script/LLM_NPC.EWeekdayFlags"))
	int32 OpeningDays = 0;

	UPROPERTY(EditAnywhere, Category="Market", meta=(ClampMin=0, ClampMax=23))
	int32 OpenHour = 8;

	UPROPERTY(EditAnywhere, Category="Market", meta=(ClampMin=0, ClampMax=24))
	int32 CloseHour = 18;

	UPROPERTY(EditAnywhere, Category="Market")
	TArray<FMarketGood> Goods;

	UPROPERTY(EditAnywhere, Category="Market")
	TObjectPtr<ANPCCharacter> Vendor;

	// === Stand-Slots: der Pfeil IST der Slot (Position + Blickrichtung in einem) ===
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Market")
	TObjectPtr<UArrowComponent> VendorSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Market")
	TObjectPtr<UArrowComponent> CustomerSlot;

	FTransform GetVendorStandTransform()   const;
	FTransform GetCustomerStandTransform() const;

	bool HasVendorStand()   const { return VendorSlot   != nullptr; }
	bool HasCustomerStand() const { return CustomerSlot != nullptr; }

	// === Kundenplatz-Reservierung (ein Kunde gleichzeitig) ===
	bool IsCustomerSlotFreeFor(uint32 _entity) const { return m_customerOccupant == 0 || m_customerOccupant == _entity; }
	bool TryReserveCustomer(uint32 _entity);
	void ReleaseCustomer(uint32 _entity);

	// === Abfragen ===
	UFUNCTION(BlueprintPure, Category="Market")
	bool IsOpenToday() const;

	UFUNCTION(BlueprintPure, Category="Market")
	bool IsOpenNow() const;

	UFUNCTION(BlueprintPure, Category="Market")
	bool IsVendorPresent() const;

	UPROPERTY(EditAnywhere, Category="Market", meta=(ClampMin=0.0))
	float VendorPresentRadius = 250.0f;

	UFUNCTION(BlueprintPure, Category="Market")
	int32 GetPrice(int32 _goodIndex) const;

	// Preis abhängig von der Sympathie des Verkäufers zum Käufer (_buyerLiking in
	// [0,1], 0.5 = neutral). Höhere Sympathie → günstiger, niedrige → teurer.
	UFUNCTION(BlueprintPure, Category="Market")
	int32 GetAdjustedPrice(int32 _goodIndex, float _buyerLiking) const;

	// Wie stark die Sympathie den Preis verschiebt (max. Rabatt/Aufschlag in %).
	// 0.4 = bei voller Sympathie −40 %, bei voller Abneigung +40 %.
	UPROPERTY(EditAnywhere, Category="Market", meta=(ClampMin=0.0, ClampMax=0.9))
	float PriceSensitivity = 0.4f;

	// Spieler kauft Ware _goodIndex (Gold abziehen, Item gutschreiben, Bestand senken,
	// Gold an den Vendor)
	UFUNCTION(BlueprintCallable, Category="Market")
	bool TryBuy(ALLM_NPCCharacter* _buyer, int32 _goodIndex);

	UFUNCTION(BlueprintCallable, Category="Market")
	bool TryBuyForNPC(ANPCCharacter* _buyer, int32 _goodIndex);

	UFUNCTION(BlueprintPure, Category="Market")
	TArray<FInventorySlot> GetOfferSlots() const;

	UPROPERTY(EditAnywhere, Category="Market|UI")
	TSubclassOf<UShopWidget> ShopWidgetClass;

	// === Interactable ===
	virtual void OnInteract(AActor* _interactor) override;
	virtual FString GetInteractionPrompt() override;

	virtual bool CanInteract() const override { return IsOpenNow() && IsVendorPresent(); }
	virtual bool UsesChatUI() const override { return false; }          // öffnet Shop, keinen Chat

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Market")
	TObjectPtr<USceneComponent> m_root;

	UPROPERTY(VisibleAnywhere, Category="Market")
	TObjectPtr<UStaticMeshComponent> m_mesh;

	UPROPERTY(VisibleAnywhere, Category="Market")
	TObjectPtr<USphereComponent> m_interactSphere;

	uint32 m_customerOccupant = 0;

	UPROPERTY()
	TObjectPtr<UShopWidget> m_openShop;

	UFUNCTION()
	void OnSphereBegin(UPrimitiveComponent* _comp, AActor* _other, UPrimitiveComponent* _otherComp,
		int32 _bodyIndex, bool _bFromSweep, const FHitResult& _sweep);
	UFUNCTION()
	void OnSphereEnd(UPrimitiveComponent* _comp, AActor* _other, UPrimitiveComponent* _otherComp,
		int32 _bodyIndex);

	FString OpeningDaysText() const;
};
