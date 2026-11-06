# Devlog – 2026-07-21

Drei Bögen: **(1)** Der letzte offene Punkt der LLM-Schicht – **grammar-constrained
Output** für alle JSON-Antworten – plus die Latenz-Ursache ab der zweiten Anfrage.
**(2)** Bewegung & Markt: Wandern über Landscape-Layer statt Fixpunkt, der
Marktplatz-Clustering-Bug, und NPCs, die **von sich aus einkaufen gehen**.
**(3)** Die komplette **Inventar-/Shop-UI in C++** statt im Blueprint-Graph.
(Fortsetzung nach #94.)

---

# 95-Eintrag: Grammar-constrained Output (`LLMSchemas`)

##### Ziel

Das Parsing der LLM-Antworten war auf Wohlverhalten des Modells angewiesen. Ein
fehlendes Komma oder ein Fließtext-Vorwort ließ eine Antwort komplett fallen. Stand
seit #79 im Backlog.

##### Umgesetzt

- **`LLM_Server/LLMSchemas.h`** (neu): eine Funktion je Antworttyp –
  `Dialogue()`, `Schedule()`, `Conversation()`, `Reflection()`, `Opinion()` –, die
  das JSON-Schema als String liefert. Enums sind mit ausformuliert (z. B. erlaubt
  `Schedule()` explizit `"shop"`).
- **`FLLMRequest::JsonSchema`** (neues Feld): jeder Aufrufer setzt beim Absenden
  `req.JsonSchema = LLMSchemas::X()`. Betrifft `UNPCBrainComponent` (Schedule,
  Conversation, Reflection, Opinion) und `UChatWidget` (Dialogue).
- **Beide Worker** (`HermesWorker`, `QwenWorker`) parsen das Schema und hängen es als
  `json_schema` an den Request-Body. llama.cpp erzwingt daraus eine Grammatik –
  ungültiges JSON kann das Modell damit gar nicht mehr erzeugen.

##### Nebenfix

Hermes bekam bisher ein `{` als Primer ans Ende des Prompts, um JSON zu erzwingen.
Mit Grammatik hätte das zu `{{` geführt. Der Primer ist jetzt an eine leere
`JsonSchema` gekoppelt:

```cpp
const bool bConstrained = !_request.JsonSchema.IsEmpty();
formattedPrompt += FString::Printf(
    TEXT("<|im_start|>user\n%s\n<|im_end|>\n<|im_start|>assistant\n%s"),
    *_request.UserMessage, bConstrained ? TEXT("") : TEXT("{"));
```

---

# 96-Eintrag: LLM-Latenz ab der zweiten Antwort

##### Ziel

Die erste Antwort kam zügig, ab der zweiten dauerte es auffällig lange.

##### Ursache

Beide llama.cpp-Server liefen mit genau **einem** Slot. Jede weitere Anfrage –
Dialog, Schedule, Gespräch, Reflexion – musste warten, bis die laufende fertig war.
Weil im Hintergrund ständig NPC-Anfragen liefen, traf der Spieler-Dialog fast immer
auf eine besetzte Pipeline.

##### Umgesetzt

- **`HermesServerManager` / `QwenServerManager`**: `--parallel 2 --cont-batching`,
  Kontext entsprechend auf `context * parallel` erhöht. Zwei Slots, kontinuierliches
  Batching – der Spieler-Dialog muss nicht mehr auf eine Hintergrund-Reflexion warten.
- **Hintergrund-Last gedrosselt**: NPC-Anfragen (Reflexion, Meinung, Gespräche) feuern
  nicht mehr ungebremst.

---

# 97-Eintrag: Wandern über Landscape-Layer & der Marktplatz-Klumpen

##### Ziel

Wandernde NPCs sollten sich auf Wegen/Plätzen bewegen statt querfeldein – und **nicht**
an einem festen Punkt hängen.

##### Umgesetzt

- **`WanderAvoidSurface`** (`UPhysicalMaterial`) + `WanderRadius` an `ANPCCharacter`.
  `WanderPointIsOnSurface()` schießt einen Line Trace mit `bReturnPhysicalMaterial`
  nach unten und verwirft Punkte auf dem Gras-Layer. `FindWanderTarget()` versucht 12
  Punkte, Mindestabstand `WanderRadius * 0.5`.
- **`bWanderAroundMarket` und `WanderPlaceChance` entfernt.** Das Wandern orientiert
  sich an keinem Ort mehr, nur noch an der Oberfläche.
- Modul **`PhysicsCore`** in die `Build.cs` (für `UPhysicalMaterial`).

##### Der eigentliche Bug

Dass sich alle NPCs exakt in der Marktplatzmitte sammelten, lag **nicht** am NavMesh
und nicht am fehlenden Phys Material – beides war korrekt. Ursache war der
Ortsmitte-Fallback am Ende von `ChooseNextDestination()`: er wies jedem NPC dieselbe
`chosen.Position` zu. Zwei Änderungen:

1. Der Wander-Zweig **kehrt jetzt zurück**, statt in den Fallback durchzufallen.
2. Das Ortsmitte-Ziel wird gestreut:

```cpp
FVector targetPos = chosen.Position;
if (UNavigationSystemV1* navSys = UNavigationSystemV1::GetCurrent(GetWorld()))
{
    FNavLocation scattered;
    if (navSys->GetRandomReachablePointInRadius(chosen.Position, 350.0f, scattered))
        targetPos = scattered.Location;
}
```

> Lehre fürs Protokoll: Ich hatte zweimal auf NavMesh bzw. Phys Material getippt,
> beides war widerlegt. Der Fehler saß in der Zielauswahl, nicht in der Navigation.

---

# 98-Eintrag: NPCs gehen von selbst einkaufen

##### Ziel

Der Markt sollte nicht nur Kulisse sein: NPCs entscheiden selbst, dass sie etwas
brauchen, laufen hin, stellen sich an einen Platz und kaufen.

##### Umgesetzt

- **`ENPCDesire::Shop`** (`NPCWorldEnums.h`) und **`ESpotType::Shop`** (`WorldSpot.h`).
  `BuildScheduleContext()` erlaubt dem LLM jetzt `"shop"` als Tagesplan-Aktion.
- **`ChooseNextDestination()`**: neuer Shop-Zweig, der einen `ESpotType::Shop`-Spot am
  `EWorldLocationTag::Market` reserviert – damit hat jeder NPC einen eigenen Standplatz
  statt einer gemeinsamen Position.
- **`AMarketStall`**: `TryBuyForNPC(ANPCCharacter*, int32)` und `GetOfferSlots()`
  (index-gleich zu `Goods`, damit UI-Auswahl und Kauf denselben Index meinen).
- **`StartingGold = 25`** am NPC, wird in `BeginPlay` ins ECS-Inventar übernommen –
  ohne Gold kein Handel.
- `AMarketStall::OnInteract` öffnet für den Spieler das `UShopWidget` mit
  `FInputModeGameAndUI` + Cursor.

---

# 99-Eintrag: Inventar- und Shop-UI als C++-Klassen

##### Ziel

Erst war die UI als Blueprint-Anleitung geplant. Verworfen zugunsten eigener
C++-Widget-Klassen – weniger Graph-Arbeit, testbar, und die Logik liegt dort, wo auch
das Inventar liegt.

##### Umgesetzt

- **`UItemRowWidget`** (`UI/ItemRowWidget.*`): eine Zeile. `SetSlot(slot, index,
  bShowPrice)` füllt Icon/Name/Info, `SetSelected(bool)` markiert,
  `FOnItemRowSelected OnSelected` meldet den Klick nach oben.
- **`UInventoryWidget`** (`UI/InventoryWidget.*`): Gold + Liste, mit virtuellem
  `GetSource(outSlots, outGold)` als einziger Datenquelle.
- **`UNpcInventoryWidget`**: erbt davon und überschreibt **nur** `GetSource()` – liest
  das Inventar des anvisierten NPCs (`GetTargetedNPC()`). Kein weiterer Code.
- **`UShopWidget`**: Angebotsliste, Zeilenauswahl, **ein** Kauf-Button unten statt
  eines Buttons je Zeile (auf ausdrücklichen Wunsch), `CloseShop()` gibt die Steuerung
  per `FInputModeGameOnly` zurück.

Alle Bindings sind `BindWidgetOptional`, alles Befüllen passiert in C++. Die WBPs
brauchen deshalb **keinen einzigen Node im Event Graph** – nur korrekt benannte
Designer-Elemente und `RowWidgetClass` in den Class Defaults.

---

# 100-Eintrag: Startausstattung des Spielers & ein stiller Fehlschlag

##### Umgesetzt

- **`StartingGold = 100`** (Default angehoben) und neu **`StartingItems`**
  (`TMap<FName,int32>`, im Konstruktor mit `bread` 3, `herb` 2, `ore` 1 belegt).
  Beides wird in `BeginPlay` ins Inventar übernommen.
- **Roh-Log** direkt nach dem Übernehmen – unabhängig von GameMode und Widget:

```
[Inventory] Spieler-Start: 100 Gold | StartingItems=3 -> Inventar=3 Art(en)
[Inventory]    bread x3
```

##### Gefundener Bug

`GetInventorySlots()` gab **ein leeres Array** zurück, sobald der aktive GameMode kein
`ALLM_NPCGameMode` war – kommentarlos, obwohl das Inventar gefüllt war. Da
`MyLLM_NPCGameMode.uasset` gelöscht wurde, traf das potenziell zu. Jetzt baut die
Funktion ersatzweise Slots aus Id + Anzahl und loggt eine Warnung, statt das Inventar
zu verschlucken.

---

# 101-Eintrag: Refresh über Fingerabdruck statt Rebuild

##### Ziel

Wunsch war „alle 5 Sekunden aktualisieren". Das Widget aktualisierte aber bereits alle
0,5 s – Refresh war nie das Problem. Der eigentliche Mangel lag darin, **wie** es
aktualisierte.

##### Vorher

```cpp
ItemList->ClearChildren();          // alles wegwerfen
for (jedes Item)
    CreateWidget<UItemRowWidget>(...);   // neu erzeugen
```

Bei jeder Änderung wurden sämtliche Zeilen zerstört und neu allokiert: Auswahl weg,
Scrollposition weg, dauernder Allokationsdruck – und ein leeres Inventar erzeugte
schlicht **null Zeilen**.

##### Jetzt

`NativeTick` bildet alle 0,5 s einen Fingerabdruck aus Gold + allen Item-Ids und
-Anzahlen und zeichnet nur bei echter Änderung neu. Damit erscheinen neue Items
praktisch sofort statt nach bis zu 5 s, und die Auswahl überlebt.

```cpp
uint32 sig = GetTypeHash(gold);
for (const FInventorySlot& s : slots)
{
    sig = HashCombine(sig, GetTypeHash(s.ItemId));
    sig = HashCombine(sig, GetTypeHash(s.Count));
}
```

> Compiler-Notiz: `::GetTypeHash(FName)` schlägt fehl. `FName`s Überladung ist ein
> *hidden friend* und nur per ADL auffindbar – der `::`-Qualifizierer blendet genau
> sie aus. Ohne `::` schreiben.

---

# 102-Eintrag: Festes Slot-Raster mit Belegt-Status

##### Umgesetzt

- **`SlotCount = 20`** an `UInventoryWidget`. `EnsureSlots()` erzeugt die Zeilen
  **einmal** beim Öffnen und hängt sie in den Container.
- **`UItemRowWidget::SetEmpty()`** und **`IsFilled()`**: ein leerer Slot behält sein
  Button-Bild, hat aber keinen Text und ein verstecktes Icon (`Hidden`, nicht
  `Collapsed` – sonst springt das Layout beim Befüllen).
- Danach werden Slots nur noch beschrieben, nie neu erzeugt:

```cpp
if (_slots.IsValidIndex(i))
    row->SetSlot(_slots[i], i, /*bShowPrice*/ false);
else
    row->SetEmpty();
```

- Leere Slots sind **nicht auswählbar** (`HandleClicked` bricht bei `!m_bFilled` ab) –
  sonst ließe sich im Shop ein leerer Slot markieren und „Kaufen" drücken.

Der Shop bleibt bewusst dynamisch: dort gibt der Stand die Angebotszahl vor, ein festes
Raster wäre falsch.

---

# 103-Eintrag: Layout-Härtung der Zeilen

##### Drei Fehler, die sich als „UI kaputt" zeigten

- **Button unsichtbar.** `SetSelected(false)` setzte den Tint auf
  `FLinearColor(1,1,1,0.1f)`. `SetBackgroundColor` ist ein **Multiplikator auf das
  Designer-Brush**, kein eigener Hintergrund – das Button-Bild wurde damit zu 90 %
  durchsichtig gerechnet, in jedem Slot. Jetzt `FLinearColor::White` als neutraler
  Wert; nur die Auswahl färbt ein.
- **Icon-Größe unkontrolliert.** Neu `IconSize` (`FVector2D`, Default 32×32) +
  `SetDesiredSizeOverride()` in `NativeConstruct` und `SetSlot`. `bMatchSize` steht
  jetzt konsequent auf `false` – die Texturauflösung soll gerade nicht durchschlagen.
- **Zeilen unterschiedlich breit.** Der Layout-Slot entsteht erst beim `AddChild` und
  ist im Designer nicht einstellbar; ohne `HAlign_Fill` bestimmte jede Zeile ihre
  Breite aus dem Inhalt. Neu `UItemRowWidget::StretchInParent(UPanelSlot*)`, angewendet
  in `EnsureSlots()` **und** `UShopWidget::Refresh()`.

##### Diagnose-Logs

`BindWidgetOptional` scheitert bei Namensfehlern **lautlos**. Beide Widgets loggen
deshalb beim Aufbau, was tatsächlich gebunden wurde:

```
[Inventory] WBP_Inventory_C -> TxtGold=OK ItemList=OK RowWidgetClass=OK
[Inventory] BuildList: Gold=100, 3 Item(s)
[ItemRow] Binding fehlt: ImgIcon=NULL ...      (nur im Fehlerfall)
```

---

# Stand & offene Punkte

##### Noch nicht verifiziert

- Die Inventar-Anzeige zeigte zuletzt **0 Gold und keine Items**, obwohl DataAssets und
  GameMode gesetzt waren. Die Diagnose-Logs aus #100/#103 sind genau dafür eingebaut,
  ein bestätigender Durchlauf steht aus. Wahrscheinlichster Kandidat: Konstruktor-
  Defaults kommen über **Live Coding** nicht an – CDO-Änderungen brauchen einen
  vollen Rebuild.

##### Editor-Setup (offen)

- 14 **`ItemDataAssets`** anlegen + in `GameMode.KnownItems`; ohne sie greift der
  Fallback (rohe Id, kein Icon/Preis).
- **WBPs** bauen: `WBP_ItemRow`, `WBP_Inventory`, `WBP_NpcInventory`, `WBP_Shop`,
  `WBP_SpeechBubble`, `WBP_DoorPrompt` – und die Klassen zuweisen.
  Im `WBP_ItemRow` muss das **Canvas Panel gelöscht** werden (Canvas hat keine
  Auto-Höhe → unsichtbare Zeilen).
- **12 BT-Tasks** im BT-Asset verdrahten; Decorator-Werte brauchen den vollen String
  (`ENPCDesire::Work`, nicht `Work`).
- **`PM_Grass`** auf die Gras-LayerInfo, `WanderAvoidSurface` am NPC setzen.
- **Marktstände** platzieren (Goods je Stand), Shop-Spots am Marktplatz.

##### Aus dem Backlog erledigt

- ~~Grammar-constrained / JSON-Schema-Output für alle LLM-Antworten~~ → #95
- ~~Inventar-Widgets (Spieler + NPC)~~ → #99 (Code steht, WBPs offen)
