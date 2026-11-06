#pragma once

#include "CoreMinimal.h"

// ============================================================================
//  FInventoryData – Inventar + Gold.
//  Plain-Struct (kein UObject), damit es sowohl als ECS-Komponente eines NPCs
//  als auch direkt am Spieler-Character genutzt werden kann. Items werden über
//  eine stabile ID (FName, = UItemDataAsset::ItemId) referenziert.
// ============================================================================
struct FInventoryData
{
	TMap<FName, int32> Items;   // Item-ID → Anzahl
	int32 Gold = 0;

	// Anzahl hinzufügen (negativ = abziehen). Fällt die Anzahl auf 0, wird der
	// Eintrag entfernt.
	void AddItem(FName _id, int32 _count = 1)
	{
		if (_id.IsNone() || _count == 0) return;
		int32& n = Items.FindOrAdd(_id);
		n += _count;
		if (n <= 0) Items.Remove(_id);
	}

	// Entfernt genau _count; true, wenn genug vorhanden war (sonst unverändert).
	bool RemoveItem(FName _id, int32 _count = 1)
	{
		int32* n = Items.Find(_id);
		if (!n || *n < _count) return false;
		*n -= _count;
		if (*n <= 0) Items.Remove(_id);
		return true;
	}

	int32 GetItemCount(FName _id) const
	{
		const int32* n = Items.Find(_id);
		return n ? *n : 0;
	}
	bool HasItem(FName _id, int32 _count = 1) const { return GetItemCount(_id) >= _count; }

	void AddGold(int32 _amount) { Gold = FMath::Max(0, Gold + _amount); }

	// Zieht _amount ab, wenn genug Gold da ist. true = gekauft/bezahlt.
	bool SpendGold(int32 _amount)
	{
		if (_amount < 0 || Gold < _amount) return false;
		Gold -= _amount;
		return true;
	}
};
