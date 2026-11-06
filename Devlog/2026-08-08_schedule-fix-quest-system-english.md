# Devlog – 2026-08-08

Großer Block seit dem letzten Eintrag: Das **LLM-Schedule-Fluten** ist behoben und die
Tagesplanung folgt jetzt dem Park-et-al.-Modell (einmal/Tag + Replan nur bei neuer,
wichtiger Info). Darauf aufbauend steht ein **komplettes Quest-System**: LLM-Vergabe im
Dialog, ein aufklappbares Quest-Log, ein **NPC-Quest-Board mit Abgabe-Button** und
NPC-Antwort im Chat. Zum Schluss läuft das ganze Spiel konsequent auf **Englisch** (fürs
LLM einheitlich). Fortsetzung nach #123.

Der C++-Teil ist überall kompiliert; die WBP-Arbeit + Klassenzuweisungen stehen unter
„Stand & offene Punkte". Alle neuen `UPROPERTY`/Klassen sind **nicht** Live-Codeable →
zum Übernehmen Editor zu → rebuild → auf.

---

# 124-Eintrag: LLM-Schedule-Fluten – Cooldown + In-Flight-Guard

##### Ziel

NPCs fluteten die lokalen LLM-Server mit `[X] LLM-Schedule angefragt` – dutzende Anfragen
pro Spielstunde.

##### Umgesetzt

- `UNPCBrainComponent::RequestSchedule(bool _bForce)` ist jetzt ein **gedrosselter**
  Einstieg: eine Routine-Anfrage wird geschluckt, solange schon eine läuft
  (`m_scheduleRequestID` gesetzt = In-Flight) **oder** seit der letzten Anfrage weniger
  als `ScheduleCooldownSeconds` (UPROPERTY, Default 30s) vergangen ist.
- Das eigentliche Absenden ist in `DispatchSchedule()` ausgelagert; die internen Timer
  (Server-Warten, Retry) zeigen dorthin und umgehen so die Guards.
- **Force** (umgeht beide Guards): Bootstrap in `BeginPlay`, Tageswechsel in `Tick`,
  Quest-Intent, gelernte Info.
- Verifiziert im Log: Anfragen liegen jetzt ~30 s auseinander statt kontinuierlich.

---

# 125-Eintrag: Routine-Replan aus der BT-Schleife entfernt

##### Ziel

Der BT-Knoten „Plan Day" rief pro Aktionszyklus `ReplanSchedule()` → Fluten **und**
Plan-Thrashing (jeder Zyklus lieferte einen anderen, nichtdeterministischen Plan).

##### Umgesetzt

- `UBTTask_PlanDay::ExecuteTask` ist ein reiner `Succeeded`-No-op.
- Tagesplanung läuft nur noch **ereignisgesteuert** (Bootstrap + Tageswechsel + Events),
  die schon vorhanden waren. Jeder NPC behält seinen Tagesplan stabil über den Spieltag.

---

# 126-Eintrag: Replan nur bei neuer, wichtiger Info

##### Ziel

Neu planen, wenn ein NPC etwas **Planrelevantes** erfährt – nicht bei jedem Geplauder
(Generative Agents / Park et al.).

##### Umgesetzt

- Schwelle `ANPCCharacter::ScheduleReplanMinImportance` (UPROPERTY, Default **0.6**).
- `OnLearnedInformation` ist das zentrale Tor: es plant nur um, wenn
  `Wichtigkeit ≥ Schwelle` – und **gedrosselt** (kein Force), damit wiederholtes
  Erzählen derselben Sache nicht doch flutet.
- **NPC↔NPC-Gerücht** (`ExchangeGossip`) darf jetzt umplanen (nur sehr wichtige
  Erinnerungen überschreiten die Schwelle). **Spieler-Gespräch**: Wichtigkeit aus dem
  Intent (Quest 0.85, Hostile 0.65, Trade 0.40, sonst 0.30) → dieselbe Schwelle.
- Verkäufer-Work-Lock (`FScheduleData::bVendorOnDuty`) im laufenden Build verifiziert:
  offener Stand hält den Verkäufer auf `Work`, kein Ziel-Flappen mehr.

---

# 127-Eintrag: Quest-Daten an die UI angebunden

##### Umgesetzt

- Am Spieler: Delegates `OnQuestAdded(const FQuestData&)` (für Einblendung) und
  `OnQuestsChanged()` (Log neu aufbauen), beide `BlueprintAssignable`.
- `AddQuest` / `TryCompleteQuestsFrom` sind jetzt `BlueprintCallable` und broadcasten.

---

# 128-Eintrag: Quest-Log als aufklappbare Karten, per Taste

##### Umgesetzt

- `UQuestLogWidget` (Container `QuestList`, optional `TxtEmpty`, `RowWidgetClass`) baut
  sich über `OnQuestsChanged` selbst neu auf – kein Polling.
- `UQuestRowWidget` ist ein **Akkordeon**: eingeklappt nur der Titel, Klick auf
  `BtnHeader` klappt `DetailsBox` (Ziel/Belohnung/Geber/Beschreibung) auf. Ereignis
  `OnExpandedChanged` fürs WBP (Chevron/Anim).
- Öffnen per Taste wie das Inventar: `ToggleQuestLog()` + `QuestLogAction` (IA), lazy
  erstellt. Beim Öffnen **Maus-Cursor an + Bewegung/Kamera gesperrt**
  (`m_bQuestLogVisible` in `Move`/`Look`), damit die Kopfzeilen klickbar sind.
- Gegen den „eingeengten" Look: Karten mit Abstand (Slot-Padding aus C++) + `Border`;
  `TxtDescription` mit „Auto Wrap Text".

---

# 129-Eintrag: „Neue Quest"-Toast

##### Umgesetzt

- `UQuestToastWidget` hängt sich an `OnQuestAdded`, blendet Titel/Ziel ~3 s ein
  (`DisplaySeconds`), dann weg; Event `OnQuestToastShown` für eine WBP-Anim.
- **Auto-erzeugt**: Spieler hat `QuestToastWidgetClass`, `BeginPlay` erstellt es +
  `AddToViewport(20)` – kein manuelles Platzieren nötig.

---

# 130-Eintrag: Test-Quests + Items zum Start

##### Umgesetzt

- `bSpawnTestQuests` (UPROPERTY, Default an) legt beim Start 3 Beispiel-Quests an –
  Old Bram/`wheat`×3, Elsy/`egg`×5, Magnus/`ore`×2 (echte Geber + Item-Ids) – **still**
  (kein Toast).
- Legt gleich die **geforderten Items** ins Inventar (Mengen aus den Quests), damit die
  Abgabe sofort testbar ist.

---

# 131-Eintrag: Quest-Vergabe per LLM

##### Umgesetzt

- Der Dialog liefert optional ein `quest`-Objekt: `LLMSchemas::Dialogue()` erlaubt es
  (Top-Level required nur reply/intent/mood), `ParseLLMResponse` füllt `FQuestData`,
  `ChatWidget` ruft `AddQuest`. Item ist auf echte Markt-Items begrenzt (Grounding über
  `GameMode::GetItemListString`), Belohnung 10–50 Gold.
- **Prompt-Fix**: das Beispiel-JSON zeigte das `quest`-Feld nicht → LLM „vergaß" die
  Quest. Jetzt **zwei Beispiele** (normal + mit Quest). `description` ist im Schema
  **Pflicht** (nie mehr leer).
- Quest fest auf **„Fetch zum Geber"** gezurrt: der Spieler bringt dem NPC selbst N×
  Item – kein Personen-Suchen, kein Ausliefern an andere, keine Orte.
- Der NPC **kennt im Dialog seine offenen Quests** (+ warum): `BuildDialogueContext`
  listet die Quests, deren Geber dieser NPC ist (Titel/Rest/Belohnung/Beschreibung) und
  weist an, sich darauf zu beziehen und nicht dieselbe doppelt zu geben.

---

# 132-Eintrag: Vergabe nur bei Trigger-Wort

##### Ziel

Eine Quest soll nur kommen, wenn der Spieler gezielt danach fragt (deterministisch).

##### Umgesetzt

- Harte Sperre im Code: `ChatWidget` nimmt eine LLM-Quest nur an, wenn die letzte
  Spieler-Nachricht ein Wort aus `QuestTriggerWords` enthält (EditAnywhere, Default
  **work/job/task/quest/help/hire**, case-insensitive; `PlayerAskedForWork()`). Sonst
  wird die Quest verworfen (Log).
- Der Prompt fordert dieselben Wörter („ONLY if the player's message contains …").

---

# 133-Eintrag: NPC-Quest-Board mit Abgabe-Button

##### Umgesetzt

- `UNpcQuestBoardWidget` zeigt beim Reden die Quests **dieses** NPCs (gefiltert auf
  `GiverName == GetTargetedNPC()`), jede als `UNpcQuestRowWidget` mit `BtnTurnIn`.
- Neue Spieler-Methode `TryCompleteQuest(FString questId)` löst **genau eine** Quest ein
  (Items da → weg, +Gold, raus; sonst false).
- Die **automatische Abgabe beim Ansprechen** (`TryCompleteQuestsFrom` in Interact/
  StartConversation) wurde entfernt – Abgabe läuft jetzt bewusst über den Button.

---

# 134-Eintrag: Exklusive Ansichten (behebt den toten Abgabe-Button)

##### Ziel

Das Chat-Widget (Z 10) lag über dem Board (Z 5) und fing die Klicks ab → der
Abgabe-Button war nicht drückbar.

##### Umgesetzt

- Während der NPC-Interaktion ist **immer nur ein Panel** sichtbar. Kein Überlappen →
  das sichtbare Panel bekommt die Klicks.
- Spieler-Methoden `ShowNpcQuestBoard()` / `ShowNpcInventoryOnly()` / `BackToChatView()`.
- Chat-Buttons `BtnQuests` / `BtnInventory` öffnen die jeweilige Ansicht (Chat wird
  versteckt); Board- bzw. Inventar-`BtnBack` gehen zum Chat zurück.
- NPC-Inventar wird nicht mehr automatisch mit dem Chat eingeblendet (Standard = nur
  Chat).

---

# 135-Eintrag: Abgabe-Antwort landet im Chat

##### Umgesetzt

- Bei erfolgreicher Abgabe postet das Board die NPC-Antwort ins **Chat-Fenster**:
  `player->PostNpcChatLine()` → `UChatWidget::AddNpcMessage()` (Nachrichtenliste +
  Verlauf). Beim „Zurück" steht die Zeile im Chat.
- Das Board-`TxtResponse` bleibt optionales Sofort-Feedback; ein Fehlschlag („fehlt
  noch …") landet nur am Board, nicht im Chat.

---

# 136-Eintrag: Alles auf Englisch + English-only im Prompt

##### Ziel

Einheitliche, für das LLM einfachere Sprache – kein Deutsch mehr im LLM- oder
Spieler-sichtbaren Text.

##### Umgesetzt

- Alle deutschen LLM-/spieler-sichtbaren Strings → Englisch: Test-Quests
  (Titel/Beschreibung), Abgabe-Antworten, Toast-Fallback, Chat-Labels („You"/„Error" …),
  Tür-Prompt, Marktstand-Prompt + Öffnungstags-Kürzel, **Wochentage** in der UI-Uhr
  (`ClockWidget`).
- **English-only** im Prompt: `BuildDialogueContext` beginnt mit „Think and write ONLY in
  English …" + verschärfte RULES-Zeile; die System-Prompts für NPC↔NPC-Gespräch,
  Reflexion und Meinung enthalten „in English only". Der Spieler darf deutsch schreiben –
  der NPC **versteht** es, **antwortet** aber englisch.
- Bewusst **nicht** angefasst: die deutschen `UE_LOG`-Debug-Zeilen und Code-Kommentare
  (keine „Antworten").

---

# 137-Eintrag: Sprechblasen-Diagnose (NPC↔NPC)

##### Umgesetzt

- Die C++-Kette (LLM-Gespräch → `SetConversationSubtitles` → `ConversationGestureTick` →
  `ShowSpeechLine` → `USpeechBubbleWidget`) ist vollständig. Falls keine Blasen
  erscheinen, sagt der Log jetzt **warum**: `Dialogzeilen=0` (Offline/keine LLM-Zeilen),
  „kein Widget-Objekt" (`SpeechBubbleWidgetClass` am `BP_NPCCharacter` leer), „erbt NICHT
  von USpeechBubbleWidget" (WBP-Parent falsch), „kein TextBlock 'TxtLine'".
- Fix ist damit **editor-seitig** (Klasse am `BP_NPCCharacter` setzen, WBP mit `TxtLine`).

---

# 138-Eintrag: Keine doppelten Quests

##### Ziel

Ein NPC soll nicht mehrmals dieselbe Quest vergeben (z. B. auf wiederholtes Fragen).

##### Umgesetzt

- `ALLM_NPCCharacter::AddQuest` verwirft eine **Dublette**: gleicher Geber UND – bei
  Fetch dasselbe Ziel-Item, bei Delivery derselbe Ziel-NPC (case-insensitive).
- Test-Quests (#130) laufen an `AddQuest` vorbei → unberührt. Der Geber weiß im Dialog
  ohnehin schon von seinen offenen Quests (#131, #137-Kette) – das ist jetzt die **harte**
  Absicherung dahinter.

---

# 139-Eintrag: Boten-/Nachrichten-Quests (Delivery)

##### Ziel

Quests, bei denen der Spieler eine Nachricht von NPC A zu NPC B trägt; B **merkt sich**
die Nachricht, reflektiert und **antwortet per LLM**.

##### Umgesetzt

- **Datenmodell**: `FQuestData` bekam `EQuestType Type {Fetch, Delivery}` + `TargetNpc`;
  bei Delivery ist `Description` der Nachrichtentext. Schema-Quest-Objekt
  (`LLMSchemas::Dialogue`) um `kind` (fetch|delivery) + `target` erweitert; Pflichtfelder
  jetzt `kind/title/description/reward` (item/count/target optional).
- **Vergabe per LLM**: `BuildDialogueContext` kennt beide Arten, baut per
  `TActorIterator<ANPCCharacter>` die Namen der ANDEREN Dorfbewohner als gültige
  Boten-Ziele (Delivery nur angeboten, wenn es welche gibt) und liefert ein eigenes
  Delivery-Beispiel-JSON. Boten-Quest nur bei **Trigger-Wort** (wie Fetch), Ziel ≠ Geber.
  `ParseLLMResponse` liest `kind`/`target`. Der Geber weiß im Dialog auch von den
  Boten-Quests, die er selbst ausgegeben hat.
- **Abgabe beim Ziel-NPC – dasselbe Board, kein neues WBP**: `UNpcQuestBoardWidget::Refresh`
  zeigt beim Ziel-NPC eingehende Boten-Quests (Zeile „Message from A: …"). Der
  Abgabe-Button routet auf `ALLM_NPCCharacter::DeliverMessageQuest`: Belohnung gutschreiben,
  Quest raus, dann beim Ziel-NPC `OnLearnedInformation(msg, 0.7, A, bReplan=true)` →
  **Gedächtnis + Reflexion + ggf. Replan** (0.7 ≥ Schwelle 0.6, gedrosselt), plus eine
  Beziehungs-Erinnerung an **A** (reflect zu ihm). Danach antwortet der Ziel-NPC **per LLM
  im Chat** und es wird dorthin zurückgewechselt.
- **LLM-Antwort**: `UChatWidget::TriggerDeliveryReply` schickt die überbrachte Nachricht
  als synthetische Spieler-Zeile (bewusst OHNE Trigger-Wort → erzeugt keine neue Quest)
  über den neuen, aus `SendMessage` extrahierten gemeinsamen Kern `SubmitUserTurn`.

##### Nebenfix

- Die Dublette-Prüfung (#138) deckt Delivery gleich mit ab (Geber + Ziel-NPC).
- `#include "EngineUtils.h"` in `LLM_NPCCharacter.cpp` (für `TActorIterator`).
- **C2181 „else ohne if"**: `UE_LOG` expandiert zu einem `{}`-Block – in einem
  brace-losen `if/else` macht das folgende `;` daraus ein leeres Statement vor dem `else`.
  Beide Zweige geklammert. (Kostete eine Build-Runde.)

---

# 140-Eintrag: Quest-Schema erzwingt Pflichtfelder (oneOf) + mehr Hermes-Kontext

##### Ziel

Zwei Fehler aus `Saved/LLMResponses`: Fetch-Quests kamen **ohne `item`** zurück (still
verworfen) oder das Quest-JSON wurde **abgeschnitten** (`finish_reason:"length"`).

##### Umgesetzt

- Ursache 1 war meine Schema-Lockerung aus #139 (item/count optional → das 3B-Modell
  lässt sie weg). Fix: die Quest im Dialog-Schema (`LLMSchemas::Dialogue`) ist jetzt ein
  **`oneOf` mit `kind`-`const`** – „fetch" erzwingt `item`+`count`, „delivery" erzwingt
  `target`. `description` (längstes Feld) steht **zuletzt**, damit die strukturierten
  Felder auch bei knappem Token-Budget vollständig sind.
- Ursache 2 war das enge Kontextfenster: Prompt ~900 + Antwort > `n_ctx` 1024. Fix:
  Hermes `context` in `Deploy/LLMServer/ServerConfig.ini` **1024 → 2048** (Pro-Slot-
  Kontext = config × parallel / Slots). `tokens` steht auf 512, war also nicht der Engpass.

##### Nebenfix

- `oneOf`/`const` wird vom llama.cpp-`json_schema` (Worker senden es als `json_schema`)
  in eine Grammatik übersetzt. Für kleine Modelle gilt: Pflichtfelder **erzwingen**, nicht
  nur „erlauben".

---

# 141-Eintrag: Mehr Quest-Trigger-Wörter (für Boten-Quests)

##### Umgesetzt

- `QuestTriggerWords` (ChatWidget) um **message, deliver, errand, letter, tell, send**
  erweitert (zu work/job/task/quest/help/hire). Der Prompt-Wortsatz zieht synchron nach
  und sagt: diese Wörter deuten auf eine **`delivery`**-Quest.
- Hinweis bleibt: der Check ist case-insensitive Substring (`tell`/`send` triggern auch in
  Alltagssätzen); die Liste ist ein `EditAnywhere`-Feld und ohne Rebuild kürzbar.

---

# 142-Eintrag: Quest-Titel + Row-Text kürzen

##### Ziel

Im Quest-Log/Board sollen die Zeilen knapp bleiben – kein langer Text, v.a. bei
Boten-Quests waren die Titel zu lang.

##### Umgesetzt

- Titel per `AbbrevTitle` auf 30 Zeichen mit „…" (Log- **und** Board-Zeile). Im Log ist
  die Delivery-Zielzeile jetzt „Deliver to X" – der volle Nachrichtentext steht nur in den
  Details (`TxtDescription`), nicht mehr in der Zielzeile.
- Kopfzeilen-Felder (Title/Objective/Reward) sind einzeilig mit **Ellipsis + Clip-to-
  Bounds** (`ClipOneLine`) → sie schneiden **an der Border-Breite** ab (im WBP je Text eine
  Border mit ~10px Innenabstand = „Border − 20"). `TxtDescription` bleibt umbruchfähig.
- Der Prompt fordert außerdem einen **kurzen Titel** (≤ 4 Wörter, kein Satz) an der Quelle.

---

# 143-Eintrag: Boten-Abgabe entschärft + Empfänger nach guter Beziehung

##### Ziel

Beim Abgeben einer Boten-Quest kam als „Antwort" ein (oft kaputter) Schedule; außerdem
sollen NPCs Nachrichten eher an Leute geben, mit denen sie gut stehen.

##### Umgesetzt

- `DeliverMessageQuest` löst **keinen sofortigen Schedule-Replan** mehr aus
  (`OnLearnedInformation(..., bReplan=false)`): der frühere `bReplan=true` feuerte bei der
  Abgabe einen eigenen LLM-Schedule-Call, der mit der gesprochenen Antwort konkurrierte und
  Müll-Pläne lieferte (im Log als `Hermes_<npc>_…json`-Schedule sichtbar). Der Ziel-NPC
  **merkt sich die Nachricht + reflektiert** weiterhin; die Info wirkt beim nächsten
  regulären Plan.
- Boten-Ziel bevorzugt **gute Kontakte**: `BuildDialogueContext` baut zusätzlich eine
  `friendList` (andere NPCs mit `Liking ≥ 0.6`), die Delivery-Regel sagt „prefer someone
  you are on good terms with, such as …". Zu Spielbeginn sind alle neutral (0.5), die
  Bevorzugung greift also erst mit gewachsenen Beziehungen.

---

# 144-Eintrag: Proaktive Begrüßung – der NPC eröffnet das Gespräch (über Qwen)

##### Ziel

NPCs sollen von sich aus anfangen: schon eine Eröffnungszeile bereit haben, wenn der
Spieler kommt – und das **nicht über Hermes** (das bleibt für den Spieler-Dialog frei).

##### Umgesetzt

- **Erzwungene Qwen-Route**: neues Feld `bPreferQwen` in `FLLMRequest`; `SubmitRequest`
  schickt solche Anfragen direkt an Qwen statt der Hermes-first-Logik. (Es gab vorher
  **keine** Server-Wahl pro Request – nur Hermes-first/Qwen-Fallback; daher landeten
  Schedules manchmal auf Hermes.)
- **Begrüßung**: kleines `LLMSchemas::Greeting()` (nur `reply`); Brain `RequestGreeting()`
  baut einen kurzen Prompt (passt in Qwens 768-Kontext), gedrosselt (In-Flight + 30s-
  Cooldown + nur wenn keine wartet), Ergebnis via `SetPendingGreeting`. `ChatWidget::
  OpenForNPC` zeigt die Zeile als **erste NPC-Nachricht** (einmalig, `ConsumePendingGreeting`).
- **Auslöser = Nähe, mit Vorlauf**: separate, größere `AwarenessSphere`
  (`m_awarenessSphere`, Radius über UPROPERTY `GreetingRange`, Default 700) neben der
  200er-Interaktions-Sphäre → die Zeile wird schon vorbereitet, während der Spieler noch
  zuläuft (Generieren dauert). Die Interaktions-Sphäre stößt sie nur noch als gedrosselter
  Fallback an.
- **Quest-bewusst**: hat der Spieler eine offene Quest von genau diesem NPC, fragt die
  Begrüßung nach dem **Fortschritt** (Fetch: Menge×Item, Delivery: Ziel-NPC) statt einer
  allgemeinen Floskel. Weiterhin **keine** Quest aus der Begrüßung (Trigger-Wort-Gate bleibt).

---

# Stand & offene Punkte

##### Editor-Setup (offen) – WBPs + Klassen

- **Quest-Log**: `WBP_QuestRow` (Parent `QuestRowWidget`: `Border→VBox→[BtnHeader→
  TxtTitle] + [DetailsBox→TxtObjective/TxtReward/TxtGiver/TxtDescription]`, `DetailsBox`
  default Collapsed); `WBP_QuestLog` (Parent `QuestLogWidget`: Box `QuestList`,
  `RowWidgetClass=WBP_QuestRow`).
- **Toast**: `WBP_QuestToast` (Parent `QuestToastWidget`: `CanvasPanel→Border→VBox`
  mit `TxtTitle`/`TxtObjective`, Anchor = Position).
- **NPC-Quest-Board**: `WBP_NpcQuestRow` (Parent `NpcQuestRowWidget`: `TxtTitle`/
  `TxtObjective`/`TxtReward` + `BtnTurnIn`); `WBP_NpcQuestBoard` (Parent
  `NpcQuestBoardWidget`: `QuestList` + optional `TxtNpcName`/`TxtResponse`/`TxtEmpty` +
  `BtnBack`; `RowWidgetClass=WBP_NpcQuestRow`). **Dient auch der Boten-Abgabe (#139) –
  kein zusätzliches WBP nötig.**
- **Chat-WBP** (`WBP_LLMChatWidget`): Buttons `BtnQuests` und `BtnInventory` ergänzen.
- **NPC-Inventar-WBP**: Button `BtnBack` ergänzen.
- **Am Spieler-BP** setzen: `QuestLogWidgetClass`, `QuestToastWidgetClass`,
  `NpcQuestBoardWidgetClass`, `QuestLogAction=IA_QuestLog`. **`IA_QuestLog`** anlegen +
  Taste (z. B. J) im `IMC_Default` mappen.
- **Sprechblase**: `SpeechBubbleWidgetClass` am `BP_NPCCharacter` auf dein
  `WBP_SpeechBubble` (TextBlock `TxtLine`).
- **Proaktive Begrüßung (#144)**: braucht **kein** WBP. Nach dem Rebuild erscheint am
  `BP_NPCCharacter` die zweite `AwarenessSphere` + der Regler `GreetingRange` (Default 700)
  – Radius dort visuell justierbar.
- **Server**: `Deploy/LLMServer/ServerConfig.ini` hat jetzt Hermes `context=2048` – wirkt
  erst nach **Server-Neustart** (passiert beim Editor/Game-Neustart). Bei knappem VRAM auf
  1536 senken.

##### Bekannte Kanten

- Der Trigger-Wort-Check ist **Substring** (case-insensitive) – „homework" würde „work"
  treffen. Wortliste `QuestTriggerWords` ist editierbar.
- Spieler-Wichtigkeit für Quest/Info-Replan wird **aus dem Intent** abgeleitet (kein
  echter LLM-Score). Feiner ginge ein eigenes `importance`-Feld im Dialog-JSON.
- Boten-Quest: Nur der **Ziel**-NPC lernt/reagiert; der Absender A erfährt (noch) nicht,
  dass seine Nachricht angekommen ist. (Das fehlende `item` aus #139 ist mit dem `oneOf`-
  Schema in #140 behoben.)
- Proaktive Begrüßung (#144): In einem dichten Dorf lösen beim Durchlaufen mehrere
  Awareness-Sphären je eine Qwen-Vorbereitung aus (jeweils nur 1×/30s, Low-Priority) –
  etwas Verschnitt. Bei Bedarf `GreetingRange` senken. Angezeigt wird die Begrüßung erst
  beim Öffnen des Chats (Sprechblase wäre eine Alternative, braucht das offene Bubble-WBP).
- Der Smith-Arbeitsspot bei `(-7780, 16660, 1280)` liegt mit **Z=1280 über dem Navmesh**
  → `MoveTo … Result=0`. Editor-seitig auf das Navmesh setzen.
- Build: neue `UPROPERTY`/Klassen sind **nicht** Live-Codeable → Editor-zu-Rebuild.
  Läuft im Editor eine **Live-Coding-Sitzung**, blockiert sie den Command-Line-Build
  („Unable to build while Live Coding is active" → `Ctrl+Alt+F11` oder Editor schließen).
