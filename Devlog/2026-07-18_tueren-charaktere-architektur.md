# Devlog – 2026-07-18

Zwei große Bögen: **(1)** Welt- & Interaktions-Ausbau – schwenkende Türen mit
NPC-/Spieler-Logik, ein zentraler Sound-Manager, modulare Quaternius-Charaktere und
Anim-Feinschliff. **(2)** Ein großer **Architektur-Abgleich**, der den Code auf das
Thesis-Dokument bringt: Weltzustand-Schicht + Prompt Builder, das komplette
`UNPCBrainComponent`, Reflexion (Park-Trias) und ein aufgeräumtes ECS.
(Fortsetzung nach #79.)

---

# 80-Eintrag: Türen-System (`ADoor`)

##### Ziel

NPCs sollen Türen glaubwürdig öffnen/schließen, der Spieler bewusst per Taste.

##### Umgesetzt

- **`ADoor`** (`World/Door.*`): Aufbau `Root → Hinge → DoorMesh`, sanftes Schwenken
  per `FInterpTo`, Tick schaltet sich am Ziel selbst ab.
- **NPC hält an & öffnet bewusst** (`bNPCStopAndOpen`): beim Erreichen der Tür wird der
  laufende `MoveTo` über das PathFollowing **pausiert** (`PauseMove`/`ResumeMove`) – der
  NPC bleibt stehen, die Tür geht auf, dann läuft er weiter. Kein BT-Umbau nötig.
- **Spieler per Taste** (`bPlayerMustPressButton`): über der Tür erscheint ein
  Prompt-Widget (`WidgetComponent`, Screen-Space), Interact-Taste → öffnen/schließen +
  optionale Geste (`DoorGestureMontage`), Bewegung für die Gestendauer gesperrt.
- **Navigation**: `SetCanEverAffectNavigation(false)` am Türblatt → der NavMesh läuft
  durch den Rahmen, NPCs pathen bis zur Tür (sie öffnet rechtzeitig davor).

##### Nebenfix

- Die LLM-Server-Prüfung in `ToggleChat` blockierte **jede** Interaktion. Jetzt hinter
  den Nicht-Chat-Zweig gezogen → Türen/Marktstände funktionieren auch, während die
  Server noch starten.

---

# 81-Eintrag: Sound-Manager (`USoundManager`)

##### Umgesetzt

- **`USoundManager`** (`Audio/`, `UGameInstanceSubsystem` → garantiert Einzel-Instanz
  ohne eigenen Singleton-Code): `PlaySoundAtLocation` (3D), `PlaySound2D` (UI), plus
  Lautstärke-**Kategorien** (SFX/UI/Ambient) + Master, statischer `Get()`-Helfer.
- **Tür-Sounds** laufen darüber (`OpenSound`/`CloseSound`).
- **Räumlich begrenzt**: optionale `SoundAttenuation` wird durchgereicht → die Tür ist
  nur **in der Nähe** hörbar (Falloff), nicht global.

---

# 82-Eintrag: Modulare Charaktere (Quaternius)

##### Ziel

Bewohner datengetrieben aus Basiskörper + Kleidung + Haaren zusammensetzen – ein
NPC-Blueprint für alle.

##### Umgesetzt

- **`UNPCDataAsset`** erweitert (Kategorie *Appearance*):
  - `BodyMesh` (Basiskörper), `OutfitPieces` (alle **geriggten** Teile: Kleidung UND
    Frisur/Bart/Brauen – folgen per **Leader Pose** der Körperanimation),
  - `AnimClass` (pro Skelett, z. B. männlich/weiblich → `ABP_Male`/`ABP_Female`),
  - `Attachments` (starre **Static Meshes** an einem Kopf-Socket, für Props ohne Rig).
- **`ApplyModularAppearance`** in `BeginPlay`: setzt Body + AnimClass, hängt Outfit-Teile
  an und koppelt sie via `SetLeaderPoseComponent`, steckt Static-Attachments an den Socket.
- **Outline-Fix**: `SetHighlighted` legt CustomDepth jetzt auf Body **+ alle** Outfit-/
  Anbauteile (vorher nur der Basiskörper → Kleidung ohne Umrandung).

##### Asset-Pipeline

- Import über **glTF** (einheitlicher Weg → gleiches Skelett für alle Teile, sonst bricht
  Leader Pose). Gegen durchblitzende Haut („poke-through") wird der Basiskörper in Blender
  auf **Kopf (+ Hände)** reduziert – die Kleidung liefert den Rest.

---

# 83-Eintrag: Talk-Animationen – Feinschliff

##### Problem

Rede-Idle und Antwort-Montage liefen parallel (verschiedene Slots), und kurze Antworten
schnitten die Geste sofort ab.

##### Umgesetzt

- Beim Umschalten wird die jeweils **andere** Talk-Montage aktiv gestoppt (nicht mehr auf
  Slot-Verdrängung verlassen).
- Die Antwort-Geste läuft **mindestens einmal komplett**, bevor es zurück zur Rede-Idle
  geht (`m_bTalkReplyEndPending`).
- Der Selbst-Loop im Blend-Out ist gegen das gezielte Stoppen abgesichert (kein
  Stottern).

---

# 84-Eintrag: Wandern ums Dorfzentrum

##### Umgesetzt

- **`bWanderAroundMarket`**: beim Umherwandern wird der Zufallspunkt um den **Marktplatz**
  (Dorfzentrum) gesucht statt um die eigene Position (`GetRandomReachablePointInRadius`
  mit dem Markt-Ort als Zentrum). Fällt sauber auf „um sich" zurück, wenn kein Markt-Ort
  gesetzt ist. So bleibt das Leben sichtbar im Zentrum.

---

# 85-Eintrag: Weltzustand-Schicht + Prompt Builder  *(Architektur)*

##### Ziel

Kap. 8 des Architektur-Dokuments real machen: eine orthogonale Weltzustands-Schicht, die
ein Prompt Builder automatisch vor jeden Prompt stellt.

##### Umgesetzt

- **`UWorldStateProvider`** (`World/`) – drei Ebenen: **Zeit** (WorldClock),
  **Orte** (WorldLocationManager), **Wirtschaft** (aktuell offene Marktstände).
  `BuildBlock()` liefert den `WORLD STATE:`-Präfix.
- **`FPromptBuilder`** (`NPC/`): stellt den Weltzustand **vor** den NPC-Kontext.
  `GenerateHermesPrompt`/`GenerateSchedulePrompt` delegieren dorthin; der NPC-Teil steckt
  jetzt in `BuildDialogueContext`/`BuildScheduleContext`.
- Damit „wissen" NPCs automatisch Tag/Zeit und welche Stände offen sind – Kontext, den
  kein einzelner NPC aus seinen Komponenten ableiten kann.

---

# 86-Eintrag: `UNPCBrainComponent` – LLM-Logik herausgelöst  *(Architektur)*

##### Ziel

Kap. 3: das „Gehirn" als eigene `UActorComponent`, die **alle** LLM-Methoden bündelt –
Separation of Concerns real statt nur im Text.

##### Umgesetzt

- **`UNPCBrainComponent`** (`NPC/Components/`): sitzt auf jedem NPC, kennt seinen NPC
  (EntityID) und hält die LLM-Kommunikation. Schrittweise, kompilierbar herausgelöst:
  - **Tagesplanung** (Anfrage → Antwort → ins ECS laden, inkl. Server-Warten/Retry),
  - **Dialog-Anwendung** (`ApplyDialogueResult`: Emotion/Memory/Blackboard + Replan),
  - **NPC↔NPC-Gespräche** (LLM-Anfrage, Ausgang, JSON-Log, Offline-Formel),
  - **LLM-JSON-Parsing** (`ParseLLMResponse` inkl. Quest).
- **Character** behält nur schlanke Forwarder + die Nicht-LLM-Teile (Talk-State, Gesten,
  Partner-Suche, Gossip). Das **ChatWidget parst kein JSON mehr selbst**, sondern nutzt
  `ParseLLMResponse` (Parsing raus aus der UI-Schicht).

---

# 87-Eintrag: Reflexion – Generative-Agents-Trias komplett  *(Architektur)*

##### Ziel

Nach Park et al. (2023): der NPC zieht aus angesammelten Ereignissen höhere Einsichten.

##### Umgesetzt

- **`FReflectionData`** ist keine tote Struktur mehr: sammelt die **Wichtigkeit** neuer
  Ereignisse an; überschreitet sie den Schwellwert (3.0), fragt das Gehirn eine Reflexion
  an (async, kein Doppel via `bRequestInFlight`).
- **Gehirn**: `BuildReflectionPrompt` (8 wichtigste Erinnerungen) → LLM liefert 2–3
  Einsichten → als **hochgewichtige** `Insight:`-Erinnerungen zurückgeschrieben.
- **Wirkung**: über `GetTopMemories` fließen die Einsichten in künftige Dialog-/Plan-
  Prompts → sie **lenken das Verhalten**. Damit: Erinnern + Planen + **Reflektieren**.

---

# 88-Eintrag: ECS aufgeräumt + Dokument-Abgleich  *(Architektur)*

##### Umgesetzt

- **`FReflectionData`** und **`FConversationData`** in den `GetComponent`-Dispatch
  aufgenommen (waren nicht erreichbar → praktisch tot). `FConversationData` wird jetzt
  auch **befüllt**: `Begin`/`End` beim Gesprächsstart/-ende (beidseitig), die LLM-
  Dialogzeilen landen im Verlauf. Alle **14 Komponenten** sind jetzt erreichbar.
- **Textangleichungen fürs Dokument** ausgeschrieben: korrekte Komponentenzahl (14),
  reale BT-Task-Anzahl (5 C++-Tasks), Blackboard-Präzisierung (BT liest nur, das LLM
  schreibt via ECS), und die ehrliche ECS-Beschreibung (Array-of-Structs pro Entität
  statt `TComponentStore`).

---

# 89-Eintrag: Gesprächs-Kontinuität in Prompts

##### Ziel

NPCs sollen sich auf frühere Gespräche beziehen – gegenüber dem Spieler und
untereinander – statt jedes Mal bei null zu starten.

##### Umgesetzt

- **Spieler-Dialog**: `BuildDialogueContext` enthält jetzt das jüngste NPC-Gespräch aus
  `FConversationData::ConversationHistory` → der NPC kann beim Reden mit dem Spieler auf
  sein letztes Schwätzchen mit einem anderen Dorfbewohner anspielen.
- **NPC↔NPC-Kontinuität**: neuer Pro-Partner-Puffer `PastConversations`
  (`TMap<EntityID, …>`). Gespeichert wird, **wenn der Dialog ankommt**
  (`OnConversationResponse`, für beide Seiten), gelesen **am Start** des nächsten
  Gesprächs (`BuildConversationPrompt`) → Abschnitt „Last time you two spoke:". Damit
  bauen zwei NPCs über Treffen hinweg aufeinander auf.
- **Sauber getrennt**: `ConversationHistory` = letztes Gespräch (Spieler-Kontext, wird
  bei `Begin()` zurückgesetzt); `PastConversations` = Kontinuität je Partner (überlebt
  den Reset). Speicher minimal (ein Eintrag je Partner, wird überschrieben).

---

# 90-Eintrag: Gespräche → Beziehungs-Erinnerungen (verdichtet)

##### Ziel

Nicht den Wortlaut horten, sondern die **Essenz** behalten – was ein NPC über eine
Person aus Gesprächen „weiß".

##### Umgesetzt

- Das Gesprächs-LLM liefert im selben JSON eine verdichtete **`memory`**-Zeile (kein
  zweiter Call).
- Sie wird bei **beiden** NPCs als Beziehungs-Erinnerung abgelegt: **`FNPCRelation::
  Memories`** (Gist pro Person, cap 3, älteste raus) – semantisch **in der Beziehung**.
- **Genutzt**: NPC↔NPC-Prompt („What X remembers about Y from past talks") **und**
  Spieler-Dialog (Erinnerungen je bekannter Person unter „People you know").
- Der rohe Pro-Partner-Verlauf (`PastConversations` aus #89) ist damit **ersetzt**;
  `ConversationHistory` bleibt nur noch für den Spieler-Kontext „gerade geredet mit …".
- **Zusammenspiel**: Ereignisse → Reflexion (Einsichten), Gespräche →
  Beziehungs-Erinnerungen (Gist pro Person) – ein gemeinsames, verdichtendes
  Gedächtnismodell.

---

# 91-Eintrag: Meinung pro Person (zweite Verdichtungsstufe)

##### Ziel

Aus den einzelnen Gesprächs-Erinnerungen eine **stabile Meinung/Haltung** pro Person
destillieren – die zweite Stufe über den Erinnerungen.

##### Umgesetzt

- **`FNPCRelation`** bekommt **`Opinion`** (stabile Meinung) + `MemoriesSinceOpinion`
  (Trigger-Zähler; `AddMemory` zählt mit).
- Nach **3 neuen Erinnerungen** mit einer Person fragt das Gehirn async eine Meinung an:
  das LLM bekommt aktuelle Meinung + Liking/Trust + die jüngsten Momente und destilliert
  **einen Satz** (`{"opinion":"…"}`) → `RequestOpinionUpdate` / `OnOpinionResponse`.
- **Genutzt**: NPC↔NPC-Prompt (Meinung + jüngste Momente) und Spieler-Dialog
  (`(your view: …)` je bekannter Person).
- **Gestuftes Gedächtnis**: Rohgespräch (`ConversationHistory`) → Gesprächs-Erinnerung
  (`Memories`, Gist) → **Meinung** (`Opinion`, stabil) → Reflexion (Einsichten über alles).

##### Design-Hinweis

- Die Meinung bildet der **Initiator** über den Partner (dessen Gehirn den LLM-Response
  verarbeitet) → asymmetrisch, gleicht sich aber über viele Gespräche aus.

---

# 92-Eintrag: Sprechblasen für NPC↔NPC-Gespräche

##### Ziel

Die emergenten NPC-Gespräche **sichtbar** machen – beim Zuschauen soll man lesen können,
worüber zwei NPCs reden.

##### Umgesetzt

- **`USpeechBubbleWidget`** (BindWidget-TextBlock „TxtLine") + `WidgetComponent`
  (Screen-Space) über jedem NPC, standardmäßig versteckt.
- Das Initiator-Gehirn gibt die LLM-Dialogzeilen an den Initiator
  (`SetConversationSubtitles`); der bestehende **Gesten-Takt** (`ConversationGestureTick`,
  2 s) zeigt die nächste Zeile **über dem tatsächlichen Sprecher** (Namensabgleich),
  blendet die andere Blase aus – synchron zur Antwort-Geste.
- **Ende**: beide Blasen aus, Untertitel geleert.
- **Einrichtung**: WBP mit Parent `SpeechBubbleWidget` + `TxtLine`, am NPC-BP
  `SpeechBubbleWidgetClass` zuweisen. Leer = keine Blasen (läuft wie bisher).

##### Timing

- Die erste Geste ist noch ohne Blase (LLM async); die Zeilen erscheinen ab ~1–3 s im
  2-s-Takt. Antwortet das LLM langsamer als die Gesprächsdauer (~6 s), bleibt es bei
  reiner Körpersprache (best-effort).

---

# 93-Eintrag: BT-Tasks für Tür, Markt, Gespräch

##### Ziel

Die inhaltliche Doc-Lücke (5 statt 12 BT-Tasks) angehen: Tür, Markt und NPC-Gespräch als
echte Behavior-Tree-Tasks.

##### Umgesetzt

- **`UBTTask_Converse`** (Dialog): startet ein NPC↔NPC-Gespräch
  (`TryStartNPCConversation`) und wartet, bis es endet; kein Partner → Success.
- **`UBTTask_OpenDoor`** (Navigation/Interaktion): öffnet die nächste Tür in Reichweite,
  wartet bis offen (Timeout-gesichert), per-Instanz für sicheren State.
- **`UBTTask_BuyAtMarket`** (Navigation/Interaktion): kauft am nächsten offenen Stand die
  erste bezahlbare Ware → **NPCs nehmen an der Wirtschaft teil**
  (`AMarketStall::TryBuyForNPC`).
- **Support**: `TryStartNPCConversation` (Character) + `bAutoStartConversations`
  (Tick-Trigger abschaltbar, wenn der BT die Gespräche übernimmt).
- Jetzt **8 C++-Tasks** (Richtung 12): Nav/Interaktion (GoToLocation, OpenDoor,
  BuyAtMarket), Dialog (Talk, Converse), Hilfstasks (Idle, CheckNeeds, PerformAction).

##### Offen

- Die Tasks müssen im **BT-Asset** (Editor) in die passenden Zweige verdrahtet werden;
  `Converse` hinter einen Cooldown/Decorator legen.

---

# 94-Eintrag: Wander, Reflect, PlanDay, DecideGoal – 12/12 BT-Tasks

##### Ziel

Die restlichen BT-Tasks bauen (Doc-Zahl 12 tatsächlich erreichen) und das Wandern an die
**Landscape-Oberfläche** koppeln statt an einen Standpunkt.

##### Umgesetzt

- **`UBTTask_Wander`** (Nav): zufälliger erreichbarer Punkt, **meidet das Gras-Layer**
  per Bodentrace aufs Physical Material (`AvoidSurface`) → NPCs laufen auf Wegen/Stein
  statt querfeldein.
- **`UBTTask_Reflect`** (LLM-Komm): stößt eine Reflexion an, wenn `ImportanceSum ≥
  MinImportance` (z. B. in Ruhephasen).
- **`UBTTask_PlanDay`** (LLM-Komm): `ReplanSchedule()` aus dem BT (z. B. am Tagesbeginn).
- **`UBTTask_DecideGoal`** (Hilfstask): `ChooseNextDestination()` als Node.
- **Support**: `TriggerReflection()` (Character-Wrapper).
- Damit **12/12** C++-Tasks in vier Gruppen: **Nav/Interaktion** (GoToLocation, OpenDoor,
  BuyAtMarket, Wander), **Dialog** (Talk, Converse), **LLM-Kommunikation** (Reflect,
  PlanDay), **Hilfstasks** (Idle, CheckNeeds, PerformAction, DecideGoal).

##### Editor

- Dem **Gras-Layer-Info** ein Physical Material (`PM_Grass`) geben, am `Wander`-Task
  `AvoidSurface = PM_Grass` → dann läuft das Oberflächen-Wandern.
- Die 12 Tasks im **BT-Asset** in die passenden Zweige verdrahten.

---

# Aktueller Stand

Die Welt ist interaktiver (Türen, Sound, modulare Bewohner) – aber der eigentliche
Sprung liegt in der **Architektur**: Der Code deckt sich jetzt mit dem Thesis-Dokument.
Alle LLM-Aufgaben liegen im `UNPCBrainComponent`, ein `WorldStateProvider` + `PromptBuilder`
erden jeden Prompt in der Welt, und mit der Reflexion ist die Generative-Agents-Trias
(Erinnern / Planen / Reflektieren) vollständig. Für die Thesis bedeutet das: die im
Dokument beschriebenen Schichten existieren real und sind sauber getrennt.

---

# Offene Punkte (Backlog)

Gesammelt über alle bisherigen Einträge – nichts davon blockiert den aktuellen Stand.

##### Quests & Ökonomie

- **Quest-UI** fehlt (aktuell nur Log + `GetActiveQuests()`); Abgabe ist automatisch,
  die NPC-Antwort **bestätigt die Erfüllung noch nicht** im Dialog.
- **Grammar-constrained / JSON-Schema-Output** für *alle* LLM-Antworten (Quest, Schedule,
  Gespräch, Reflexion) → garantiert valides JSON und macht das Parsing bombensicher.

##### Generative Agents

- **Relevanz-Retrieval mit Embeddings** statt `Importance × Recency` – der letzte
  Park-Baustein (braucht einen lokalen Embedding-Server), nicht im Dokument gefordert.
- Reflexionen **optional als eigene Prompt-Sektion** („Your insights:") statt nur als
  hochgewichtige Erinnerung.
- Edge: bleibt eine Reflexions-Antwort aus, hängt `bRequestInFlight` – unkritisch, aber
  ein Timeout-Reset wäre sauberer.

##### Gespräche

- Meinungen bildet nur der **Initiator** über den Partner (asymmetrisch, gleicht sich
  über viele Gespräche aus) – strikte Symmetrie wäre möglich.
- Der **Offline-Fallback** (keine Server) erzeugt weder Erinnerung noch Meinung.
- Sprechblasen zeigen nur den **LLM-Dialog** (Offline-Fallback bleibt stumm); bei sehr
  langsamer LLM-Antwort sieht man nur die Körpersprache.

##### Behaviour Tree / Architektur

- **12/12 C++-BT-Tasks** (nach #94) – müssen im BT-Asset verdrahtet werden; fürs
  Oberflächen-Wandern braucht das Gras-Layer-Info ein Phys Material.
- **Doc-Textangleichungen** noch ins Dokument übernehmen (Vorlagen liegen vor:
  14 Komponenten, **12 Tasks**, Blackboard-Präzisierung, AoS statt `TComponentStore`).

##### Türen / Animation

- Ein aggressiv konfiguriertes **BT-`MoveTo`** könnte die Tür-Pause überschreiben (dann
  kein Anhalten, aber sauberer Durchlauf) – bei Bedarf als NavLink/BT-Task härten.
- **„Tür öffnen"-Animation** (Asset) für NPC und Spieler noch zu besorgen.

##### Editor-Setup (noch nicht in der Engine erledigt)

- 14 **`ItemDataAssets`** anlegen + in `GameMode.KnownItems`, `StartingGold` setzen.
- **Marktstände** platzieren; Input Actions + Widget-Klassen zuweisen.
- **UMG**: Inventar-Widgets (Spieler + NPC), Wochentag-Textfeld (`TxtWeekday`),
  Tür-Prompt (`WBP_DoorPrompt`).
