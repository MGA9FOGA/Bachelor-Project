# Devlog – 2026-06-17

NPC-Ablauf zum Laufen gebracht: AIController/BT verdrahtet, Needs-Modell umgebaut, Schedule-Hänger behoben, Pause entkoppelt, Crash gefixt. (Fortsetzung nach \#23 – Tagesablauf im DataAsset.)


# 24-Eintrag: AIController & Behavior Tree fest verdrahtet

##### Ziel

Sicherstellen, dass jeder NPC seinen Controller + BT bekommt, ohne dass man es pro Blueprint vergessen kann.

##### Umgesetzt

- ANPCCharacter-Konstruktor: AIControllerClass = ANPCAIController + AutoPossessAI = PlacedInWorldOrSpawned.

- Neues Feld BehaviorTreeAsset direkt am ANPCCharacter (Kategorie „AI") – der BT lässt sich jetzt **am NPC-Blueprint** zuweisen (wie NPCData).

- ANPCAIController::OnPossess holt den BT vom NPC, falls am Controller keiner gesetzt ist; Fehlermeldung präzisiert.

##### Hinweis

FNPCBT (in NPCBehaviorTree.h) ist **toter Code** – nie instanziiert, nie implementiert. Benutzt wird der native UE-Behavior-Tree (BTTask-Klassen).


# 25-Eintrag: Crash-Fix – Widget-Pointer ohne UPROPERTY

##### Problem

PIE-Crash nach ~40 s: EXCEPTION\_ACCESS\_VIOLATION in CoreUObject. Ursache: m\_serverStatusInstance und m\_clockWidget waren rohe UObject-Pointer **ohne UPROPERTY()** → vom GC nicht erfasst → eingesammelt, während ihre Timer (HTTP/Health, Uhr) noch liefen → toter Pointer.

##### Umgesetzt

Beide Member mit UPROPERTY() versehen (GC-sicher).


# 26-Eintrag: ClockWidget – Spielzeit-Anzeige

##### Umgesetzt

- Neues UClockWidget (UI/ClockWidget.h/.cpp), zeigt HH:MM der UWorldClock, pollt im Timer. GetTimeString() ist BlueprintPure.

- Setup: WBP ableiten, TextBlock TxtTime anlegen, am Player einblenden.

- (Ein kurz eingebautes On-Screen-Zeit-Debug wurde wieder entfernt – Widget ersetzt es.)


# 27-Eintrag: Debug-Logs für Ablauf & Needs

##### Umgesetzt

- \[Plan\]-Log in SyncGoalToBlackboard: bei **jedem** Ziel-Wechsel (Work -\> Rest usw.), erfasst auch Needs-Overrides.

- \[Needs\]-Log in ANPCCharacter::Tick (alle ~1 s): Hunger, Erschöpfung, aktuelles Ziel und Pausing-Status – um zu sehen, ob die Pause greift.


# 28-Eintrag: Schedule-Hänger nach Rest/Eat behoben

##### Problem

Nach einem Needs-Override (Rest/Eat) blieb der NPC auf Rest/Eat hängen („danach passiert nichts mehr").

##### Ursache

Der Override setzte das Ziel, ließ aber CurrentActivityIndex stehen. Der Plan-Block aktualisiert nur „bei Wechsel" (bestIndex != CurrentActivityIndex) – blieb der Index gleich, wurde das Ziel nie zurückgesetzt.

##### Umgesetzt

Beide Override-Zweige setzen jetzt CurrentActivityIndex = -1. Danach erkennt der Plan-Block garantiert einen Wechsel und setzt das geplante Ziel neu.


# 29-Eintrag: Needs-Modell tätigkeitsbasiert

##### Ziel

Erschöpfung/Hunger sollen sich nach der **Tätigkeit** richten, nicht nach „pausiert/unterwegs".

##### Umgesetzt (FNeedsUpdateSystem)

Gesteuert über NavMesh.bIsMoving (läuft) und Waypoint.bIsPausing (am Ort):

- **Laufen/Wandern**: Erschöpfung ↑, Hunger ↑

- **Arbeiten** (am Ort): Erschöpfung ↑, Hunger ↑

- **Essen**: Hunger ↓

- **Schlafen**: Erschöpfung ↓ (schnell)

- **Ausruhen**: Erschöpfung ↓ (langsam)

- **Idle/Stehen**: alles bleibt gleich

Im Schlaf steigt die Erschöpfung **nicht** mehr (vorher stieg sie im „unterwegs"-Zweig pauschal). „Rest nur wach" regelt der Override über die Stunde (\< SleepHourThreshold ? Rest : Sleep).


# 30-Eintrag: bIsMoving-Fix für das Needs-Modell

##### Problem

Das neue Needs-Modell liest NavMesh.bIsMoving. GoToLocation setzte es bei stationären Zielen (Idle/Rest/Talk) und beim Gesprächs-Abbruch nicht auf false → Erschöpfung wäre beim Ausruhen weiter gestiegen.

##### Umgesetzt

GoToLocation setzt bIsMoving = false im frühen Succeeded-Return und beim PlayerTalking-Abbruch.


# 31-Eintrag: Pause-Logik entkoppelt

##### Problem (fragile Kopplung)

PerformAction setzte bIsPausing = true und wartete – das Zurücksetzen machte aber **nur** FScheduleSystem. Ohne aktives Schedule-System wäre die Aktion nie beendet worden (NPC eingefroren).

##### Umgesetzt

- UBTTask\_PerformAction zählt seine Pause jetzt **selbst** in der TickTask herunter und setzt bIsPausing = false, wenn die Dauer um ist.

- FScheduleSystem macht nur noch if (bIsPausing) return; (zählt nichts mehr).

- Der Pause-Zustand bleibt **pro NPC im ECS-Waypoint** – wichtig, weil BT-Node-Instanzen von allen NPCs **geteilt** werden (kein Member-State im Task!).

##### Ergebnis

PerformAction ist in sich abgeschlossen. Schedule-Flag ist weiterhin nötig – aber nur für den Plan, nicht mehr damit Aktionen enden.


# Aktueller Stand

##### Code-seitig fertig

Schedule → Override → Plan, tätigkeitsbasierte Needs, BT-Bewegung, Workstation-Reservierung, Pause-Steuerung – durchgetraced und konsistent.

##### Editor-Voraussetzungen (Pflicht, sonst läuft nichts)

- BT-Asset am NPC-Blueprint zugewiesen

- DataAsset → ActiveSystems: Schedule + TransformSync (+ Needs)

- WorldLocationManager → LocationData gesetzt

- NavMeshBoundsVolume im Level

##### Offen / nächste Schritte

- Editor-Setup fertigstellen und 2-NPC-Durchlauf gegen die Checkliste testen

- Alles in saubere Commits sichern (großer uncommitteter Stand)

- Optional: Streaming-Antworten, hardcodierte Server-Pfade → FPaths::ProjectDir()

