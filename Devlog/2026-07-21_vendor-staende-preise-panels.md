# Devlog – 2026-07-21 (Teil 2)

Der Markt wird lebendig: Verkäufer bekommen **echte Stände mit Position und
Blickrichtung**, Käufer einen eigenen Kundenplatz, Preise reagieren auf die
**Beziehung** zum Käufer, und die Interaktion ist an **Öffnungszeit + anwesenden
Verkäufer** gebunden. Dazu ein Schwung UI-Politur: NPC-Inventar beim Gespräch,
verschiebbare/skalierbare Panels mit gespeicherter Position, und Eingabe-Sperren.
(Fortsetzung nach #103.)

---

# 104-Eintrag: NPC-Inventar öffnet sich beim Gespräch

##### Ziel

Spricht man einen NPC an, soll man gleich sein Inventar sehen.

##### Umgesetzt

- Neuer Helfer `SetNpcInventoryVisible(bool)` – zeigt/versteckt gezielt (statt nur zu
  togglen). In `ToggleChat` beim Öffnen `true`, beim Schließen `false`. Der bisherige
  Toggle-Key bleibt zusätzlich erhalten.
- `RefreshInventoryInputMode()` bricht ab, solange der Chat offen ist – sonst hätte es
  dem Chat-Eingabefeld den Tastaturfokus geklaut. Der Chat hat Cursor + `GameAndUI`
  ohnehin schon aktiv, das Panel läuft als reine Anzeige mit.

##### Nebenfix

`ANPCCharacter::GetInventorySlots()` gab **ein leeres Array** zurück, sobald der aktive
GameMode kein `ALLM_NPCGameMode` war (derselbe stille Fehlschlag wie zuvor beim Spieler).
Jetzt greift ein Fallback: Slots aus Id + Anzahl, statt das Inventar zu verschlucken.

---

# 105-Eintrag: Blackboard `CurrentAction` als Kurzform

##### Ziel

`GetNameStringByValue` liefert bei einer `enum class` den voll qualifizierten Namen
(`ENPCDesire::Shop`). Decorators mussten diesen langen String exakt tippen – fehleranfällig.

##### Umgesetzt

Beim Schreiben ins Blackboard wird das Präfix abgeschnitten (`FindLastChar(':')`), sodass
dort nur noch `Shop`, `Work`, `Idle` … steht. Nur EINE Stelle geändert – `CurrentAction`
(Blackboard, von Decorators gelesen) ist ein anderes Feld als `ActiveTask` (BTState, vom
AnimBP gelesen), das unberührt bleibt.

> Konsequenz: Alle `CurrentAction`-Decorators im BT nutzen jetzt die **Kurzform** –
> `Idle`, `Wander`, `Shop` statt `ENPCDesire::…`.

---

# 106-Eintrag: Redundante Ziel-Wahl in GoToLocation entfernt

##### Ziel

Verdacht: Bewegungsdaten werden aus dem ECS-Struct genommen, aber nicht ans NavMesh
übergeben. Prüfung ergab: `MoveTo` **läuft** – aber `GoToLocation` rief
`ChooseNextDestination()` ein ZWEITES Mal auf (Decide Goal/Wander hatten das Ziel gerade
gesetzt).

##### Umgesetzt

Der Zweitaufruf ist raus. `GoToLocation` liest jetzt nur das bereits gesetzte Ziel und
schickt es an `MoveTo`. Ein Sicherheitsnetz bestimmt selbst ein Ziel, falls
ausnahmsweise keins gesetzt war.

##### Wirkung

Der `Wander`-Task **lebt** jetzt: sein bewusst gewählter Nicht-Gras-Punkt wird angelaufen
statt sofort neu ausgewürfelt. Halb so viel Reservierungs-/Suchlast pro Zyklus.

---

# 107-Eintrag: Stand-Slots am Marktstand (Verkäufer + Käufer als Pfeile)

##### Ziel

Der `Vendor` wurde nur für Gold + Prompt genutzt – er stand nirgends bewusst. Beide
Rollen sollen eine frei einstellbare Position + Blickrichtung bekommen, wie bei `AWorldSpot`.

##### Umgesetzt

- `AMarketStall` hat zwei **`UArrowComponent`-Slots**: `VendorSlot` (hinter der Theke,
  Blick zur Kundenseite) und `CustomerSlot` (davor, Blick zum Stand). Der **Pfeil IST der
  Slot** – ihn im Viewport drehen setzt direkt die NPC-Blickrichtung (kein Kind-Pfeil-Umweg).
- `GetVendorStandTransform()` / `GetCustomerStandTransform()` liefern Position + Rotation.
- In `BeginPlay` bekommt der `Vendor` seinen Stand zugewiesen (`SetAssignedStall`).
- `PerformAction` snappt den NPC per `TrySnapToStallStand()` exakt an den Slot + Blickrichtung
  (keine Spot-Montage, normale Pose aus dem AnimBP).

---

# 108-Eintrag: Käufer am Kundenplatz, Verkäufer am Posten

##### Umgesetzt

- **Käufer:** Der `Shop`-Zweig in `ChooseNextDestination` sucht den nächsten offenen Stand
  mit **freiem Kundenplatz**, reserviert ihn (ein Kunde gleichzeitig: `TryReserveCustomer`),
  läuft zum `CustomerSlot` und blickt zum Stand. Freigabe beim Aktionsende / Zielwechsel.
- **Verkäufer:** Ganz oben in `ChooseNextDestination` wird das Ziel auf **Work gezwungen**,
  solange der eigene Stand offen ist → er bleibt am `VendorSlot` und wandert/kauft nicht.
  Außerhalb der Öffnungszeit folgt er wieder seinem Tagesplan.

---

# 109-Eintrag: Interaktions-Gate + Auto-Schließen + Stapel-Sperre

##### Ziel

Der Stand soll nur ansprechbar sein, wenn er **offen** ist UND ein **Verkäufer** dahinter
steht. Der Close-Button „tat nichts".

##### Umgesetzt

- `CanInteract()` = `IsOpenNow() && IsVendorPresent()`. `IsVendorPresent()` prüft die
  Distanz des `Vendor` zum `VendorSlot` (`VendorPresentRadius`).
- Das **Shop-Widget schließt sich selbst**, sobald `CanInteract()` false wird (Ladenschluss
  oder Verkäufer geht weg) – per `NativeTick`.
- **Close-Bug gefunden:** Jedes Interagieren erzeugte ein NEUES Shop-Widget übereinander;
  Close entfernte nur das oberste. Fix: `AMarketStall` merkt sich das offene Fenster
  (`m_openShop`) und öffnet kein zweites.

##### Achtung

Der Stand ist jetzt nur interagierbar, wenn der Vendor gesetzt ist UND am Posten steht –
sonst reagiert er gar nicht. Fürs Testen `VendorPresentRadius` hochsetzen.

---

# 110-Eintrag: Verkäufer kauft nie im eigenen Shop

##### Umgesetzt

- `TryBuyForNPC` bricht ab, wenn `_buyer == Vendor`.
- Der `BuyAtMarket`-Task überspringt Stände, deren `Vendor` der NPC selbst ist.

---

# 111-Eintrag: Dynamische Preise nach Sympathie

##### Ziel

Preise sollen von der Beziehung des Verkäufers zum Käufer abhängen – Freund billiger,
Abneigung teurer.

##### Umgesetzt

- Neu **`m_playerLiking`** (0.5 = neutral) pro NPC + `GetPlayerLiking()` /
  `AdjustPlayerLiking()`; für NPC-Käufer `GetLikingToward(entity)` aus den Beziehungen.
- `ApplyDialogueResult` schiebt die Spieler-Sympathie nach der **Dialog-Stimmung**:
  Happy → +, Angry → −, Suspicious/Concerned → leicht −. (Ein Spieler-Bezug im
  Beziehungssystem existierte vorher gar nicht.)
- `GetAdjustedPrice(index, liking)` skaliert den Grundpreis: bei voller Sympathie −40 %,
  bei voller Abneigung +40 % (`PriceSensitivity`). Genutzt in `TryBuy` (Spieler),
  `TryBuyForNPC` (NPC) und `GetOfferSlots` (Anzeige).

##### Wirkung

Reden → Sympathie steigt → Preise sinken. Im Log: `… fuer X Gold (Sympathie 0.72 …)`.

---

# 112-Eintrag: „Mit Verkäufer reden" aus dem Shop

##### Umgesetzt

- Neuer optionaler **`BtnTalk`** in `UShopWidget`: schließt den Shop und öffnet den Chat
  mit dem Vendor.
- Dafür am Spieler `StartChatWith(ANPCCharacter*)` – öffnet den Chat gezielt mit einem NPC,
  unabhängig vom anvisierten Objekt.

Zusammen mit #111 die Schleife: reden → Beziehung ändern → bessere Preise beim nächsten Öffnen.

---

# 113-Eintrag: Wander meidet die Marktmitte

##### Ziel

Wandernde NPCs sammelten sich mitten auf dem Marktplatz (die größte Nicht-Gras-Fläche).

##### Umgesetzt

`FindWanderTarget` holt das Markt-Zentrum (`GetLocationByTag(Market)`) und verwirft
Wanderpunkte im `MarketAvoidRadius` (Default 700 cm). Zusammen mit dem Gras-Filter
schlendern die NPCs über die Wege, aber nicht mehr auf den Markt.

---

# 114-Eintrag: Eingabe-Sperre bei offenem Inventar

##### Ziel

Bei offenem Inventar drehte sich die Kamera mit der Maus, und man konnte weglaufen.

##### Umgesetzt

`Look()` **und** `Move()` brechen jetzt zusätzlich bei `m_bInventoryVisible` /
`m_bNpcInventoryVisible` ab. Bei offenem Inventar steht der Charakter still, die Kamera
bleibt fix, Cursor + UI-Bedienung laufen weiter.

---

# 115-Eintrag: Verschiebbare & skalierbare UI-Panels

##### Ziel

Der Spieler soll Shop-/Inventar-Fenster selbst anordnen, in der Größe ändern – und beides
soll erhalten bleiben.

##### Umgesetzt

- Neue Basisklasse **`UDraggablePanelWidget`** (Shop, Inventory und NpcInventory erben davon).
- **Größe:** `PanelSize` → auf eine gebundene SizeBox `RootBox`; der Canvas-Slot wird per
  Code auf „Size To Content" gestellt, damit die Größe zählt.
- **Verschieben:** Panel auf freier Fläche ziehen (`NativeOnMouseButtonDown/Move/Up`,
  Koordinaten über `AbsoluteToLocal` – DPI-sicher). Buttons/Zeilen fangen ihre Klicks
  selbst ab, stören also nicht.
- **Resizen:** über einen optionalen Anfasser **`ResizeHandle`** (Image/Border, kein
  Button – der verschluckte den Klick; das war der Grund, warum die reine Ecken-Zone
  über den Footer-Buttons nicht ansprach).
- **Speichern:** Position UND Größe landen in `GameUserSettings.ini` `[UILayout]` unter
  einer pro Panel eindeutigen `PanelId` – bleiben über Sessions erhalten.

---

# 116-Eintrag: Shop als modales Fenster

##### Ziel

Bug: Bei offenem Shop das Inventar auf- und zumachen setzte den Input-Mode zurück – der
Shop hing weiter offen, aber man konnte wieder laufen.

##### Umgesetzt

- Neues Flag **`m_bShopOpen`** am Spieler, vom Shop-Widget gesetzt (`NativeConstruct` →
  true, `CloseShop` UND `NativeDestruct` → false).
- Solange true, sperren `ToggleInventory`, `ToggleNpcInventory`, `Move` und `Look`.
  Raus geht es **nur** über den Close-Button (bzw. Auto-Close bei Ladenschluss / „Reden").
- Umgekehrt: `ToggleChat` öffnet nichts (Chat/Shop/Tür), solange ein Inventar-Panel
  offen ist – sonst hätte man per Interagieren ein zweites Fenster aufreißen können.

---

# 117-Eintrag: Chat verschiebbar + NPC-Inventar über den Chat

##### Ziel

Das NPC-Inventar ließ sich nicht anfassen, und der Chat sollte auch beweglich sein.

##### Ursache & Umgesetzt

- **NPC-Inventar unbewegbar:** Der Chat lag auf **Z-Order 10**, das NPC-Inventar auf **5**
  – der Chat fing die Klicks ab. Das NPC-Inventar geht jetzt auf **Z 11** (über den Chat),
  es öffnet sich ja zusammen mit ihm.
- **Chat beweglich:** `UChatWidget` erbt jetzt von **`UDraggablePanelWidget`** – bekommt
  `PanelSize`, Verschieben, Resize und Positions-Speicherung. `Super::NativeConstruct/
  Destruct` werden schon gerufen. Braucht im Chat-WBP eine `RootBox` (sonst wie bisher).

---

# 118-Eintrag: Panel-Feinschliff (Max-Größe, Drag nur auf der RootBox)

##### Umgesetzt

- **Maximalgröße:** `MaxPanelSize` (0 pro Achse = unbegrenzt) + `ClampPanelSize()`,
  angewendet beim Resizen, Anwenden und Laden. Fenster lässt sich nicht mehr über die
  Grenze ziehen.
- **Drag nur auf dem Panel:** Das Root-Canvas füllt den Bildschirm, dadurch startete ein
  Klick *irgendwo* einen Drag. Jetzt prüft `NativeOnMouseButtonDown` per
  `RootBox->GetCachedGeometry().IsUnderLocation(...)`, ob der Klick wirklich auf der
  RootBox liegt – sonst kein Drag. (Optional zusätzlich das Root-Canvas im WBP auf
  „Self Hit Test Invisible", damit Klicks daneben durchgehen.)

---

# 119-Eintrag: Drag/Resize robust – Ecken-Resize im Preview

##### Ziel

Verschieben ging, **Verkleinern nie** – bei allen Panels. Ursache: Die Resize-Erkennung
hing an einem separaten `ResizeHandle`, dessen Klick von der ScrollBox darunter
abgefangen wurde → der Griff wurde als „Verschieben" gewertet.

##### Umgesetzt (finaler Stand)

- **Resize an der RootBox-Ecke im Preview:** `NativeOnPreviewMouseButtonDown` erkennt die
  untere rechte Ecke der RootBox (in deren lokalen Koordinaten, `AbsoluteToLocal` +
  `GetLocalSize`). Preview läuft VOR den Kindern → greift **auch über der ScrollBox**.
  Kein separater Handle mehr nötig.
- **Move im Bubble:** `NativeOnMouseButtonDown` zieht von einer freien RootBox-Fläche;
  interaktive Kinder (Liste/Buttons) fangen ihre Klicks selbst ab.
- **Größe zählt:** Der Canvas-Slot der RootBox wird per Code auf `SetAutoSize(true)`
  gestellt, damit die SizeBox-Größe (`PanelSize`) durchschlägt.

##### Lehre

Ein Griff, der (fast) das ganze Panel abdeckt, schluckt sonst ALLE Klicks – die
Ecken-Erkennung über die RootBox-Geometrie ist deutlich robuster als ein Handle-Widget.

---

# 120-Eintrag: Cursor-Fix beim UI-Drag

##### Ziel

Beim Ziehen verschwand der Cursor und nichts bewegte sich.

##### Ursache & Fix

`FInputModeGameAndUI` hat standardmäßig **`HideCursorDuringCapture = true`**: Sobald der
Drag die Maus captured, versteckt UE den Cursor → die Screen-Position friert ein →
Delta = 0. In **allen** UI-Input-Modes jetzt `SetHideCursorDuringCapture(false)`
(`ToggleChat`, `StartChatWith`, `RefreshInventoryInputMode`, `MarketStall::OnInteract`,
`ChatWidget::OpenForNPC`).

---

# 121-Eintrag: Chat & NPC-Inventar nebeneinander

##### Ziel

Das NPC-Inventar geht zusammen mit dem Chat auf. Der Chat (eigenes Vollbild-Widget,
Z 10) lag über dem NPC-Inventar (Z 5) und blockierte Klicks/Mausrad.

##### Umgesetzt

- **Chat zurück auf `UUserWidget`:** Der Chat ist bewusst **nicht** mehr verschiebbar
  (der DraggablePanelWidget-Umbau brachte für das dicht mit interaktiven Elementen
  gefüllte Chatfenster nur Ärger). Nur Shop/Inventar/NpcInventory erben die Panel-Basis.
- **Chat `SelfHitTestInvisible`:** Beim Anzeigen wird der Chat auf `SelfHitTestInvisible`
  gesetzt (UserWidget + Root-Canvas). Er rendert normal und seine Inhalte (Eingabe/
  Button/Liste) bleiben klickbar, aber seine **freie Fläche fängt keine Klicks mehr ab**
  → das darunterliegende NPC-Inventar bleibt bedienbar.
- **NPC-Inventar auf Z 5** (unter dem Chat), damit der Chat oben und klickbar bleibt.

##### Grenze (bewusst)

Zwei **überlappende** Overlay-Widgets: Wo sich der tatsächliche Inhalt überschneidet
(Chat-Nachrichtenliste über NPC-`ItemList`), gewinnt immer das obere. Das lässt sich
nicht wegprogrammieren → die Panels müssen im WBP **auseinandergelegt** werden (Chat
unten, NPC-Inventar rechts oben).

---

# 122-Eintrag: Konsumierbare Items & Verzehr am Rest-Platz

##### Ziel

NPCs sollen Essen/Trinken bei sich tragen und es bei Hunger **am Rest-Platz verzehren**,
statt immer in die Taverne zu laufen.

##### Umgesetzt

- **`UItemDataAsset`** erweitert: `bConsumable`, `ConsumeType` (**Food**/**Drink**),
  `Nourishment` (0..1 – wie stark der Bedarf sinkt).
- **`ANPCCharacter::FindConsumable(type)`** durchsucht das Inventar (löst IDs über den
  GameMode auf) und liefert das erste passende Consumable.
- **`ChooseNextDestination` (Eat/Drink-Zweig):** Hat der NPC passendes Essen/Trinken
  DABEI → er reserviert einen **`Rest`-Spot** (zuhause, sonst nächster) statt des
  Tavernen-Essplatzes und merkt sich das Item (`m_pendingConsume`). Sonst wie bisher
  zur Taverne.
- **`PerformAction`** am Spot ruft **`TryConsumePending()`** → **`ConsumeItem`**: Item
  raus, `Hunger`/`Exhaustion` −`Nourishment`. Fällt der Hunger unter die
  Sättigungsschwelle, endet der Eat-Override von selbst.

##### Wirkung

Ein NPC kauft am Markt Brot → wird später hungrig → isst es an der Bank statt zur
Taverne zu laufen. Emergenz über dasselbe Inventar/Gold-System.

---

# 123-Eintrag: NPC-zu-NPC-Handel im Gespräch

##### Ziel

NPCs sollen sich untereinander Essen/Trinken **verkaufen** – ohne extra BT-Task oder
LLM-Last (bewusste Entscheidung: „automatisch beim Gespräch").

##### Umgesetzt

- In **`ApplyConversationOutcome`** (nach JEDEM NPC-Gespräch, LLM **und** offline) läuft
  `TryBuyConsumableFrom` in **beide Richtungen**:
  - Ist ein Partner **hungrig** (→ Food) oder **erschöpft** (→ Drink), hat **Gold**, und
    der andere besitzt so ein Item → er **kauft eins**.
  - Preis = `BasePrice`, leicht **sympathieabhängig** (Freund billiger, ±25 %, wie am
    Markt über `GetLikingToward`). Gold + Item wechseln den Besitzer.
- Log: `[Handel] Aldric kauft 'Brot' von Elsy fuer 4 Gold`.

##### Warum hier

`ApplyConversationOutcome` ist der zentrale Punkt, an dem beide Gesprächsergebnisse
(LLM + deterministischer Offline-Fallback) zusammenlaufen – ein Hook deckt beide Wege
ab, ohne das Gesprächs-Schema oder den BT anzufassen.

---

# Build-Fix am Rande

Ein Git-Worktree eines Spawned-Tasks war im Source-Baum gelandet
(`Source\…\.claude\worktrees\…`) und duplizierte alle Header → UnrealHeaderTool-Fehler
„Two headers with the same name". Worktree sauber entfernt (`git worktree remove`),
Branches blieben erhalten. Merke: Spawned-Task-Worktrees nicht aus dem Source-Ordner
heraus starten.

---

# Stand & offene Punkte

##### Editor-Setup (offen)

- **WBP-Aufbau** je Panel (auch **Chat**): `Canvas → SizeBox "RootBox" → Border → Overlay
  [Inhalt, ResizeHandle]`. `PanelId` eindeutig (`Shop`/`Inventory`/`NpcInventory`/`Chat`),
  `RowWidgetClass` setzen. `BtnTalk` optional. Root-Canvas ggf. „Self Hit Test Invisible".
- **Marktstände**: `Vendor` setzen, `VendorSlot`/`CustomerSlot`-Pfeile ausrichten,
  Vendor-Tagesplan mit `work` in der Öffnungszeit. Ohne anwesenden Vendor ist der Stand
  **nicht** ansprechbar.
- 14 **`ItemDataAssets`** + `GameMode.KnownItems`; Shop-Zweig im BT verdrahten;
  `PM_Grass` + `WanderAvoidSurface`.
- **Konsum/Handel**: essbare Items auf `bConsumable` + `ConsumeType`/`Nourishment` setzen;
  genug **`Rest`-Spots** platzieren (dort wird verzehrt); NPCs brauchen Startgold.
- Komplette Schritt-für-Schritt-Anleitung + Tests: **`Markt_Einrichtung_und_Test.md`** (Projektwurzel).

##### Bekannte Kanten

- Verkäufer isst/schläft während der Öffnungszeit nicht (Ziel ist auf Work gezwungen) –
  bei kurzen Öffnungszeiten unkritisch, bei ganztägigen Ständen zu bedenken.
- Ein Kunde pro Stand gleichzeitig (Kundenplatz-Reservierung ist Single-Slot); weitere
  Käufer weichen auf andere offene Stände aus.
- Spieler-Sympathie hat noch keinen Verfall (anders als die NPC-Beziehungen).
- **Drink** senkt `Exhaustion` (es gibt keinen eigenen Durst-Bedarf) – ein „Trinken"
  erfrischt also leicht, statt einen separaten Wert zu füllen. Bewusste Vereinfachung.

##### Aus dem Backlog erledigt

- ~~Verkäufer mit Standort/Blickrichtung~~ → #107/#108
- ~~Dynamische Preise~~ → #111
- ~~NPCs tragen/verzehren Essen, tauschen untereinander~~ → #122/#123
