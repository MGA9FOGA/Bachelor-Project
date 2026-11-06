# Devlog – 2026-07-05

Großer Block: robustere LLM-Pläne, `drink` als vollwertige Aktion, NPC-Beziehungen
die das LLM entscheidet, und ein neuer Wirtschafts-Layer – 7-Tage-Rhythmus, Markt,
Inventar/Gold und LLM-generierte Quests, die die Ökonomie-Schleife schließen.
(Fortsetzung nach #72.)

---

# 73-Eintrag: Schedule-Robustheit (wake, kein Tag-Schlaf)

##### Problem

Das LLM plante Unsinn: `wake`-Einträge (keine Tätigkeit) und `sleep` mitten am Tag
(z. B. 8:00 schlafen).

##### Umgesetzt

- `wake`/`wakeup`/`awake` werden beim Parsen **verworfen** – der NPC nimmt nach dem
  Schlafen einfach die nächste Aktion auf.
- Ein `sleep`-Eintrag außerhalb der Nacht (~19–5 Uhr) wird zu **`rest`** (Nickerchen).
- Prompt verschärft: „exactly ONE sleep at NIGHT; never during the day; no wake action."

---

# 74-Eintrag: `drink` als eigene Aktion

##### Umgesetzt

- Optionale **`DrinkMontage`** am Eat-Spot (Feld nur bei `Type == Eat` sichtbar,
  `EditConditionHides`): kommt ein NPC zum Trinken, wird sie statt der Ess-Montage
  gespielt. Leer → wie Essen.
- Trinken **erfrischt leicht** (`DrinkRefreshRate` – Erschöpfung runter, kein Sättigen).
- Die Montage wird pro Tätigkeit gewählt (`GetMontageForGoal`).

---

# 75-Eintrag: NPC-Beziehungen – das LLM entscheidet

##### Ziel

NPCs sollen sich nicht nur „gut verstehen" – manche mögen sich, andere nicht, und das
soll das **LLM** entscheiden (das Gehirn), nicht eine feste Formel.

##### Umgesetzt

- **LLM-getriebenes Gespräch**: Beim Treffen fragt der Initiator async das LLM
  (Personalities + aktuelle Sympathie) → **echter Dialog + Ausgang**
  `positive/neutral/negative`. Der Ausgang bestimmt das Sympathie-Delta.
- **Beziehungen** (`FRelationshipData`): `AdjustRelation` (Delta kann negativ sein),
  Namen statt IDs im Prompt („You are good friends with Magnus").
- **Verfall**: `DecayTowardNeutral` – ohne Auffrischung driften Freundschaften UND
  Abneigungen langsam Richtung neutral.
- **Meiden**: unter `AvoidLikingThreshold` geht der NPC dem anderen aus dem Weg
  (`FindConversationPartner` überspringt ihn). Da das LLM die Sympathie setzt, ist
  das Meiden indirekt LLM-getrieben.
- **JSON pro Gespräch** nach `Saved/NPCConversations/` (Dialog, Ausgang, Gossip,
  Beziehung). Offline-Fallback: deterministische Kompatibilitäts-Formel.

---

# 76-Eintrag: 7-Tage-Rhythmus

##### Umgesetzt

- `UWorldClock::GetWeekday()` (0–6, 0 = Montag) + `GetWeekdayName()` (englisch).
- Der **Schedule-Prompt** bekommt den Wochentag → das LLM darf die Routine je Tag
  variieren (Markttag, Ruhetag, Freunde besuchen). Da täglich neu geplant wird,
  unterscheiden sich die Pläne jetzt pro Wochentag.

---

# 77-Eintrag: Inventar & Gold

##### Umgesetzt

- **`FInventoryData`** (`Economy/InventoryData.h`) – Items (ID→Anzahl) + Gold, mit
  Helfern (`AddItem/RemoveItem/HasItem`, `AddGold/SpendGold`). Für NPCs **und** Spieler.
- **`UItemDataAsset`** – Item-Typen im Editor (ItemId, DisplayName, BasePrice, Icon).
- Als ECS-Komponente an jedem NPC (`GetComponent<FInventoryData>`), am Spieler als
  Member. Beide mit BlueprintCallable-API (`GetGold`, `AddItem`, …) + `StartingGold`.
- Items werden über eine stabile `FName`-ID referenziert → serialisierbar + vom LLM
  in Quests referenzierbar.

---

# 78-Eintrag: Markt mit Öffnungstagen

##### Umgesetzt

- **`AMarketStall`** – Stand mit Reichweiten-Sphere, ansprechbar wie ein NPC:
  - `OpeningDays` (Bitmask Mo–So, nutzt #76), `OpenHour`/`CloseHour`.
  - `Goods` (`FMarketGood`: Item, PriceOverride, Stock), optionaler `Vendor` (bekommt
    das Gold).
  - `IsOpenNow`, `GetPrice`, **`TryBuy(buyer, index)`** – Gold gegen Ware, Bestand
    runter, Vendor verdient.
- **Interaktion entkoppelt**: neues `IInteractableInterface::UsesChatUI()`. Der Stand
  gibt `false` → Interagieren öffnet **keinen Chat**, sondern behandelt den Kauf.

---

# 79-Eintrag: LLM-Quests + Ökonomie-Schleife

##### Ziel

Quests sollen im **Gespräch vom LLM** entstehen und mit Gold belohnt werden – das
Gold fließt in den Markt zurück.

##### Umgesetzt

- **`FQuestData`** – Bring-Quest (Geber, Item, Menge, RewardGold).
- **Erzeugung**: fragt der Spieler nach Arbeit, liefert das LLM im Dialog-JSON ein
  `quest`-Objekt. **Grounding**: der Prompt listet die existierenden Items
  (`GameMode.KnownItems`), damit die Quest nur nach echten, kaufbaren Items fragt.
- **ChatWidget** parst die Quest → **`Player->AddQuest`**.
- **Abgabe**: beim Ansprechen des Gebers werden erfüllbare Quests automatisch
  eingelöst (Items weg, Gold dazu) – `TryCompleteQuestsFrom`.
- Loop: **NPC-Quest → Item am Markt kaufen → beim Geber abgeben → Gold → mehr kaufen.**

##### Offen / Refinement

- Auto-Abgabe (kein Abgabe-Dialog); die NPC-Antwort erwähnt die Erfüllung noch nicht.
- Kein Quest-UI (nur Log + `GetActiveQuests()` für ein BP-Log).
- Grammar-constrained Output würde das Quest-JSON garantiert valide machen.

---

# Aktueller Stand

Die NPCs planen wochentagsabhängig, führen vom LLM entschiedene Gespräche (mit
Freundschaften, Abneigungen und Verfall) und die Welt hat einen Wirtschafts-Layer:
Markt mit Öffnungstagen, Inventar/Gold und LLM-Quests, die die Ökonomie schließen.
Für die Thesis ist das der Sprung von „Tagesplanung" zu „Planung + prozedurale,
welt-geerdete Inhalte + Ökonomie".
