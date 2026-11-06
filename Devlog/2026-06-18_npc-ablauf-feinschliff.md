# Devlog – 2026-06-18

Tagesablauf zum Laufen gebracht und feingeschliffen: BT-Tasks ticken endlich, Aktionen laufen bis zum Ziel-Wechsel, Needs in Spielzeit mit Hysterese, Schlaf/Rest-Logik und ein sauberer Interaktions-State. (Fortsetzung nach \#31.)


# 32-Eintrag: BT-Tasks ticken (bNotifyTick) – der große Hänger-Fix

##### Symptom

Im Log: Pausing=NO dauerhaft, Needs stiegen endlos (~0.01/s = bIsMoving-Rate). Die NPCs steckten für immer im MoveTo fest, PerformAction lief nie.

##### Ursache

In UE ruft die Engine TickTask **nur** auf, wenn der Task im Konstruktor bNotifyTick = true setzt. Die Tasks hatten keinen Konstruktor → TickTask wurde nie aufgerufen → jeder Task, der InProgress liefert, hing für immer (GoToLocation erkannte „angekommen" nie, PerformAction wurde nie erreicht).

##### Umgesetzt

Konstruktoren für UBTTask\_GoToLocation, UBTTask\_Idle, UBTTask\_PerformAction (und später UBTTask\_Talk) mit bNotifyTick = true + NodeName.

> Das löste gleichzeitig fast alle „Hänger"-Symptome der Vortage.


# 33-Eintrag: Aktion läuft bis Ziel-Wechsel + Log entschlackt

##### Problem

Der BT loopte: GoToLocation → Action → fertig → von vorne → neues MoveTo zum selben Ort → neue Action. Jede Sekunde. = „mehrfach ausgeführt" + Log-Spam.

##### Umgesetzt

- UBTTask\_PerformAction läuft jetzt **durch, bis sich das Ziel ändert** (Vergleich CurrentGoal vs. BTState.ActiveTask). Erst dann beendet es, der BT bewertet neu. bIsPausing bleibt währenddessen aktiv.

- FScheduleSystem darf jetzt JEDERZEIT neu entscheiden (kein if (bIsPausing) return mehr), damit der Ziel-Wechsel erkannt wird.

- Log: Sekunden-\[Needs\]-Spam entfernt; stattdessen Needs an den \[Plan\]-Log gehängt → eine saubere Zeile pro Ziel-Wechsel.


# 34-Eintrag: Needs in Spielzeit + Cadence

##### Ziel

"X mal pro Tag" essen/ruhen soll stabil sein, egal wie schnell die Weltuhr läuft.

##### Umgesetzt

- FScheduleSystem::GameHoursPerSecond (aus WorldClock.RealSecondsPerGameHour), in ANPCCharacter::Tick gefüttert. Needs rechnen jetzt in **Spielstunden**.

- FNeedsUpdateSystem neu: Anstieg/Erholung pro Spielstunde.

  - aktiv (laufen/arbeiten): Hunger + Erschöpfung ↑

  - essen: Hunger ↓ (+ leicht Erschöpfung ↓ via EatExhaustionRelief)

  - schlafen: Erschöpfung ↓ (stark) · ausruhen: ↓ (mittel)

- Raten als Felder in FNeedsData (tunbar), grob auf ~2× essen / ~1-2× ruhen pro Tag eingestellt.


# 35-Eintrag: Needs-Override mit Hysterese

##### Problem

Override löste bei \>= Schwelle aus und endete knapp darunter → Thrashing (Work↔Rest 50×/s, Erschöpfung klebte am Schwellwert).

##### Umgesetzt

- „Satisfied"-Schwellen (HungerSatisfied, ExhaustionSatisfied): einmal angefangen wird gegessen/geruht, bis der Wert SO WEIT unten ist.

- Expliziter Override-Zustand ENeedOverride \{ None, Eat, Rest, Sleep \} in FNeedsData. Nur ein ECHT über die Schwelle ausgelöster Override wird gehalten → eine GEPLANTE Sleep/Rest-Aktivität wird nicht mehr gekapert.

- **Essen hat Vorrang** (sättigt + lindert Erschöpfung), dann ggf. ruhen.


# 36-Eintrag: Schlaf/Rest – einmalige Wahl + Tag/Nacht-Fenster

##### Ziel

Sleep ist die nächtliche Hauptregeneration, Rest nur tagsüber. Kein Wechsel Sleep ↔ Rest mitten in der Erholung.

##### Umgesetzt

- Die Wahl Rest vs. Sleep fällt **einmal** beim Auslösen und wird gehalten (im Override-Zustand), nicht mehr jeden Frame neu anhand der Stunde.

- Nachtfenster mit Wrap-around um Mitternacht: Nacht = Stunde \>= SleepHourThreshold (20) ODER \< WakeHourThreshold (6). → 20–5 Uhr = Sleep, 6–19 Uhr = Rest.


# 37-Eintrag: Interaktions-/Talk-State + PlayerTalking-Bugfix

##### Bug gefunden

PlayerTalking wurde nur auf true gesetzt, NIE zurück auf false → nach dem ersten Gespräch blieb der NPC für immer im Talk-Zweig eingefroren.

##### Umgesetzt

- ANPCCharacter::SetTalking(bool, interactor) als sauberes Enter/Exit:

  - Enter: OnInteract → SetTalking(true, player)

  - Exit: Chat schließen (Player ToggleChat) **und** Spieler verlässt Reichweite (EndOverlap) → SetTalking(false)

- Neuer UBTTask\_Talk: stoppt Bewegung, dreht den NPC zum Spieler, setzt BTState.ActiveTask = "Talk" (vom AnimBP lesbar), läuft bis PlayerTalking == false.

- ChatWidget::GetActiveNPC() für den Exit beim Schließen.

##### Editor

BT-Talk-Branch: BTTask\_Idle → BTTask\_Talk ersetzen (Decorator bleibt).


# Aktueller Stand

Tagesablauf läuft glaubwürdig durch: arbeiten → müde → durchruhen → weiterarbeiten → hungrig → essen; nachts schlafen; Gespräch unterbricht sauber und danach geht's weiter. Zwei NPCs parallel ohne Konflikte.

##### Offen / Nice-to-have

- Anim-Blueprints an die States hängen (BTState.ActiveTask)

- Hermes-Startup-Poll zu kurz (Server did not become ready in time, Server läuft aber) → Poll-Versuche erhöhen

- Hardcodierte Server-Pfade → FPaths::ProjectDir()

- LLM-generierter Tagesablauf (Prompts existieren, nicht verdrahtet)

