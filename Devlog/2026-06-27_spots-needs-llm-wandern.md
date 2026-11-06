# Devlog – 2026-06-27

Großer Block nach den Schlaf-/Tag-Nacht-Einträgen: Erholungs- und Essplätze als
echte Spots (jetzt mit mehreren Plätzen pro Objekt), Needs-Feinschliff, ein
behobener Deadlock beim Verlassen des Play-Modus, reparierte LLM-Tagespläne und
organisches Umherwandern.
(Fortsetzung nach #49.)

---

# 50-Eintrag: Erholung zählt nur noch AM Spot

##### Problem
NPCs „erholten" sich im Stehen am Arbeitsplatz: Wenn der Needs-Override mitten in
einer laufenden Arbeits-Aktion auf Sleep/Rest schaltete, blieb `bIsPausing` vom
Arbeits-Spot noch `true`. Das Needs-System rechnete die Erholung sofort am Amboss
an → Erschöpfung fiel blitzschnell → der NPC lief nie zum Bett.

##### Umgesetzt
- Bei **jedem Ziel-Wechsel** wird `bIsPausing` zurückgesetzt (`SyncGoalToBlackboard`).
  Erholung zählt erst wieder, wenn `PerformAction` den NPC am **neuen, passenden
  Spot** erneut auf `bIsPausing` setzt.
- Damit läuft der NPC bei Sleep/Rest erst zum Bett/zur Bank und erholt sich dort –
  nicht mehr „im Stehen".

---

# 51-Eintrag: Erholungs-Spots (Rest) + Wander-Fallback

##### Ziel
Es soll Plätze geben, an denen man sich erholt. Ist keiner frei, wandert der NPC
und erholt sich dabei nur langsam.

##### Umgesetzt
- Neuer `ESpotType::Rest` (Bank/Stuhl). Rest ist nicht mehr „in place“, sondern
  läuft über `ChooseNextDestination`:
  - Rest-Spot am HomeTag → Taverne → **nächstgelegene freie Bank** (egal welches
    Gebäude, `ReserveNearestSpotActor`).
  - Kein Spot frei → **umherwandern**, das Needs-System rechnet langsame Erholung.
- Needs-System unterscheidet `bHasSpot`:
  - am echten Rest-Spot **schnell** (`RestRecoveryRate = 2.0`),
  - beim Wandern **langsam** (`WanderRestRecoveryRate = 0.6`),
  - Erholung beim Rest zählt **nur am Spot** (nicht schon auf dem Hinweg).

---

# 52-Eintrag: Essplätze – zum Essen/Trinken hinsetzen

##### Umgesetzt
- Beim Ziel **Eat** reserviert der NPC einen `AWorldSpot` mit `Type = Eat` in der
  **Taverne** (sonst nächstgelegener freier Eat-Spot), läuft hin und isst dort.
- Kein Essplatz platziert → Fallback auf Tavernen-Mitte (rückwärtskompatibel).
- Hinweis: „trinken“ läuft über dasselbe Eat-Ziel (kein eigenes Durst-Bedürfnis).

---

# 53-Eintrag: Spots mit mehreren Plätzen (Multi-Slot)

##### Ziel
Ein Objekt (lange Bank, Tisch, Lagerfeuer) soll mehrere NPCs aufnehmen.

##### Umgesetzt
- `AWorldSpot` bekommt **`Capacity`** (1–6) und mehrere Steh-Slots
  (`StandSlot_0..5`, Slot 0 = altes `NPCStandSlot`, rückwärtskompatibel).
- Reservierung pro Slot (`m_occupants`-Array); jeder NPC bekommt einen freien Slot,
  `GetStandTransformFor(entity)` liefert genau dessen Position → sie stehen
  nebeneinander, nicht aufeinander.
- Editor: `Capacity` setzen, die ersten N Slots platzieren (Pfeil zeigt
  Blickrichtung).

---

# 54-Eintrag: Needs-Feinschliff (Tag/Nacht, Hunger, Schlaf)

##### Umgesetzt
- **Nachts wird durchgeschlafen** – im Nachtfenster (kommt vom Schlaf-Typ) hat
  Schlaf absoluten Vorrang, kein Aufstehen zum Essen/Ausruhen.
- **Essen nur am Tag**: Hunger steigt nur beim Aktivsein (tagsüber), nachts hat
  Essen keinen Vorrang mehr.
- **Hunger langsamer**: `HungerRate 0.09 → 0.05` (wurde zu oft hungrig).
- **Schlaf-Hunger**: im Bett steigt Hunger ganz langsam (`SleepHungerRate = 0.04`)
  → der NPC wacht hungrig auf und geht morgens als Erstes essen.
- Override-Priorität sauber getrennt: Nacht→Schlaf, Tag→Essen vor Ausruhen.

---

# 55-Eintrag: Deadlock/Crash beim Verlassen des Play-Modus behoben

##### Problem
Beim Beenden von PIE fror der Editor ein bzw. stürzte ab: Der Game-Thread wartete
in `EndPlay` auf die Worker-Threads, die wiederum auf einen HTTP-Callback DES
Game-Threads warteten → zirkulärer Deadlock. Dazu griffen die HTTP-Callbacks per
Referenz auf Stack-Variablen zu (Use-after-free).

##### Umgesetzt (Hermes- und Qwen-Worker)
- **Poll-Wait statt endlosem Block**: der Worker prüft beim Warten regelmäßig
  `m_bShouldStop` → beim Beenden bricht er innerhalb von 50 ms ab.
- **Geteilter Zustand** (`FHttpReqState` via `TSharedRef`): der Callback greift
  nicht mehr auf Stack-Variablen zu; nach Abbruch (`bAborted`) tut er nichts mehr.
- **Singleton-Schutz**: `BroadcastResponse`/`RouteToQwen` laufen nur, wenn der
  Manager noch existiert (`ALLMRequestManager::Get() == mgr`).
- → Sauberes Herunterfahren, auch wenn eine LLM-Anfrage läuft.

---

# 56-Eintrag: LLM-Tagespläne repariert

##### Probleme
1. Qwen schnitt das Schedule-JSON ab (`tokens=128`) → Parse-Fehler → jeder NPC
   nutzte nur den Bootstrap (DataAsset), nie den LLM-Plan.
2. Qwen erfand ungültige Aktionen (`drink`, `tell stories`).
3. Qwen wurde nur teils auf die GPU geladen (`ngl=20`) → blieb ewig in
   „Loading model" (503).
4. Die allererste Anfrage im `BeginPlay` trifft das Modell fast immer im
   Ladezustand → 503.

##### Umgesetzt
- **Server-Config**: Qwen `ngl 20 → 99` (voll auf GPU), `context 512 → 768`,
  `tokens 128 → 256`; Readiness-Poll `15s → 60s` (Hermes + Qwen).
- **Prompt**: nur erlaubte Aktionen (`work/eat/rest/wander`), **kein** Sleep
  (erzwingt die Nacht-Logik), ≤6 Einträge, kompaktes Einzeiler-JSON.
- **Token**: Schedule-Anfrage `MaxTokens 256 → 512` (Qwen halbiert intern → ~256).
- **Normalisierung**: ungültige Aktionen → `wander`.
- **Retry**: schlägt die Anfrage fehl (z.B. 503), wird bis zu 6× alle 5 s erneut
  versucht, bevor der Bootstrap bleibt → der Start-Plan klappt, sobald das Modell
  geladen ist.

---

# 57-Eintrag: Chat-Filter-Bug

##### Problem
Hatte man mit einem NPC geredet, nahm das ChatWidget **jede** LLM-Antwort als
dessen Chat-Antwort an – auch die Schedule-Antworten der Neuplanung. Folge:
Schedule-JSON landete im Gesprächsverlauf des NPCs.

##### Umgesetzt
- `OnLLMResponse` verarbeitet nur noch die Antwort mit der **eigenen** Chat-
  RequestID. Ist keine offen (ID leer) oder passt sie nicht → ignorieren.

---

# 58-Eintrag: Organisches Umherwandern

##### Problem
Wander wählte **ein** Ziel (nur eine Orts-Mitte), der NPC lief hin und blieb dort
stehen (Pause-Timer wurde nie geprüft) → kein echtes Wandern.

##### Umgesetzt
- `PerformAction` beendet die Wander-Aktion nach der Pause (3–8 s) → der BT sucht
  ein **neues** Ziel → kontinuierliches Wandern.
- `ChooseNextDestination` mischt für ein organisches Gefühl:
  - **~40 %** (`WanderPlaceChance`): schlendert zu einem **benannten Ort**
    (Markt/Brunnen…),
  - **~60 %**: zufälliger erreichbarer **NavMesh-Punkt** in der Nähe
    (`GetRandomReachablePointInRadius`, Radius `WanderRadius = 900`).
- Beide Regler pro NPC im Editor einstellbar (Händler bummelt zu Orten, Wächter
  streift random).

---

# 59-Eintrag: Log-Aufräumung

##### Umgesetzt
- 503 „Modell lädt noch" ist jetzt eine ruhige Log-Zeile statt ERROR-Spam mit
  JSON-Dump (Hermes + Qwen).
- Erfolglose LLM-Antworten werden nur noch `Verbose` geloggt (kein
  „Qwen request failed"-Spam mehr); Erfolge bleiben sichtbar.

---

# Nebenbei (Setup, kein Code)
- Engine-Start-Problem gelöst: Die Registry zeigte `Unreal Engine\5.5` auf einen
  alten, kaputten Pfad (`D:\UE_5.5`) statt auf die echte Installation
  (`C:\Program Files\Epic Games\UE_5.5`). Daher „does not contain an Unreal
  Engine"/Kopie-Abfragen. Fix: Registry-`InstalledDirectory` auf den C:-Pfad.

---

# Aktueller Stand

NPCs leben einen (LLM- oder DataAsset-)Tagesablauf: arbeiten an Spots, essen an
Essplätzen, ruhen auf Bänken (mehrere Plätze pro Objekt möglich), schlafen nachts
typgerecht durch, und schlendern in der Freizeit organisch zwischen Orten und
Zufallspunkten umher. Die LLM-Pläne greifen jetzt (Token/Prompt/Retry), der
PIE-Exit ist stabil, und der Log ist lesbar.
