# Devlog – 2026-06-21

Orte/Spots überarbeitet (Actor-basiert mit Mesh), Bewegung über NavMesh stabil, Mehr-NPC-Interaktion per Blickkegel + Highlight, und das LLM schreibt jetzt selbst den Tagesablauf. (Fortsetzung nach \#37.)


# 38-Eintrag: Orts-/Aktivitäts-Modell sauber getrennt

##### Umgesetzt

- ELocationActivity entschlackt auf \{ Idle, Eat, Social \}. Work/Sleep raus – die laufen nicht über das Activity-Feld.

- GetLocationForDesire: **Arbeit = Ort hat Workstations**, **Schlaf = Ort hat** **SleepSpots**, Eat/Wander weiter über Activity.

- Neue Orts-Tags Home, Inn, House (ans Enum-Ende, damit alte Werte nicht verrutschen).

##### Merksatz

Was man an einem Ort *tut gehen* (Eat/Social/Idle) → Activity. Wo man *arbeitet/schläft* → Spots/Listen, kein Activity nötig.


# 39-Eintrag: Schlafplätze + Bett-Reservierung

##### Umgesetzt

- FWorldLocation.SleepSpots + ReserveSleepSpot/ReleaseSleepSpot (analog zu Workstations, generischer ReserveSpot-Kern, Key WORK\_/SLEEP\_).

- NPC reserviert bei Sleep ein Bett am HomeTag – Default **Taverne** (die meisten schlafen dort), sonst der gesetzte HomeTag.

- ReleaseCurrentWorkstation generalisiert (Work *und* Sleep).


# 40-Eintrag: NavMesh – Bewegung stabil gemacht

##### Problem

NPCs liefen nicht zu den Plätzen; MoveTo schlug sofort fehl. Zieldiagnose über neuen Log: MoveTo \<Tag\> @ (x,y,z) | Result=N (0=Failed, 1=AlreadyAtGoal, 2=Ok). Ziele lagen zu hoch über dem NavMesh (Z ~814/1149).

##### Umgesetzt

- NavigationSystem-Modul in die Build.cs (sonst Linker-Fehler bei UNavigationSystemV1).

- Ziel wird vor MoveTo **aufs NavMesh projiziert** (ProjectPointToNavigation, großzügiger Z-Extent) → erreichbar trotz zu hoher Spot-/Orts-Z.

- → Result=2, NPCs laufen los.


# 41-Eintrag: Spot-Actors mit Mesh (AWorldSpot)

##### Ziel

Statt Offset-Listen im DataAsset: platzierte Objekte (Amboss/Bett) im Level, die Position, Drehung und Animation tragen.

##### Umgesetzt

- AWorldSpot (Actor): Mesh (sichtbar), NPCStandSlot (wo der NPC steht + schaut), PlayerSlot, Type (Work/Sleep/Eat/Idle), Building-Tag, Montage + AnimTag, Editor-Pfeil. Registriert sich im BeginPlay.

- Manager: RegisterSpot + ReserveSpotActor(Building, Type, Entity).

- NPC: reserviert Actor-Spot (Work/Sleep) → läuft zum StandSlot, dreht sich, spielt die Montage. Kein DataAsset-Eintrag mehr nötig – nur Actor + Tags.

- Präzise Platzierung: kleiner AcceptRadius (10) + winzige Korrektur auf den StandSlot (kein großer Snap mehr).

##### Hinweis

Verknüpfung Spot ↔ NPC läuft rein über Building-Tag = WorkTag/HomeTag.


# 42-Eintrag: Mehr-NPC-Interaktion gefixt

##### Problem

Bei mehreren NPCs überschrieb sich m\_nearbyInteractable; verließ man eine Overlap-Sphere, wurde der Pointer genullt obwohl ein anderer NPC in Reichweite war → Interact brach ab, Chat ging an „(kein NPC)".

##### Umgesetzt

Player hält jetzt eine **Liste** (m\_nearbyInteractables); NPCs melden sich per AddNearbyInteractable/RemoveNearbyInteractable an/ab.


# 43-Eintrag: Anvisieren per Blickkegel + Highlight

##### Umgesetzt

- GetNearbyInteractable() wählt den NPC mit dem höchsten Dot zwischen Kamera-Vorwärts und Richtung zum NPC → den man **anschaut**.

- **Sichtkegel** über InteractAimThreshold (Default 0.9 ≈ 25°): nur im Kegel wird anvisiert/gehighlightet/angesprochen.

- **Highlight**: Player-Tick highlightet das Ziel; ANPCCharacter::SetHighlighted setzt CustomDepth + Stencil (HighlightStencil = 2, passend zum PP-Outline-Material).

##### Editor

Custom Depth-Stencil Pass „Enabled with Stencil" + PP-Outline-Material im PostProcessVolume.


# 44-Eintrag: LLM schreibt den Tagesablauf

##### Ziel

Die KI generiert pro NPC einen Tagesablauf (statt nur DataAsset/Hand).

##### Umgesetzt

- RequestLLMSchedule() (im BeginPlay, opt-in via bUseLLMSchedule): schickt GenerateSchedulePrompt() ans LLM (Prio Low), merkt sich die RequestID.

- OnScheduleResponse filtert per RequestID auf den eigenen NPC, parst \{ "schedule": \[ \{hour, action\}, … \] \} und lädt es ins ECS (ersetzt den Bootstrap-Plan). Kein Cross-Talk, stört den Chat nicht.

- Input fürs LLM: NPCData-\>SchedulePrefs (Routine, Wake/Sleep, Aktivitäten).

- Bootstrap-Plan (DataAsset/Fallback) überbrückt, bis die Antwort da ist.

##### Logs

LLM-Schedule angefragt → LLM-Schedule uebernommen (N) bzw. … fehlgeschlagen – Bootstrap bleibt.


# Aktueller Stand

NPCs leben einen (optional LLM-generierten) Tagesablauf, laufen über NavMesh zu platzierten Amboss/Bett-Spots, animieren dort, schlafen in der Taverne, und der Spieler visiert per Blickkegel + Outline genau einen NPC an und spricht ihn an.

##### Offen / Nice-to-have

- PlayerSlot beim Reden tatsächlich nutzen (Spieler-Positionierung/Kamera)

- Eat-Spots (aktuell nur Work/Sleep als Actor-Spots; Eat = Ortsmitte)

- Hermes-Startup-Poll erhöhen; hardcodierte Server-Pfade → FPaths::ProjectDir()

- Anim-Blueprints an die States/Montages hängen

