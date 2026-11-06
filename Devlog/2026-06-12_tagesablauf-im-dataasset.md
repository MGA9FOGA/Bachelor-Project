# Devlog – 2026-06-12 (Teil 2)

# 23-Eintrag: Tagesablauf pro NPC im DataAsset

##### Ziel

Den Tagesablauf nicht mehr im Code hardcoden, sondern pro NPC im Editor von Hand schreiben koennen. Daraus laufen die Blackboard-Keys, um Bewegung + Workstations zu testen.

##### Ausgangslage

Das Ausfuehren des Ablaufs war bereits komplett vorhanden:

- FScheduleSystem::Update liest CurrentGameHour, sucht den passenden Eintrag, setzt Waypoint.CurrentGoal (+ Needs-Override).

- SyncGoalToBlackboard() schreibt CurrentAction (Enum-Name) und IsSleeping; UBTTask\_CheckNeeds schreibt IsHungry/IsExhausted.

- BT: GoToLocation -\> ChooseNextDestination (reserviert bei Work eine Workstation) -\> MoveTo; danach PerformAction (pausiert, gibt Platz frei).

Hardcodiert war nur der Plan selbst (LoadTestSchedule), gleich fuer alle NPCs.

##### Umgesetzt

- NPCDataAsset:

  - Neues Enum ENPCScheduleActivity (Work/Eat/Sleep/Wander/Idle/Rest) – Dropdown im Editor, keine Tippfehler.

  - Neues USTRUCT FNPCScheduleEntry \{ int32 Hour (0–23); ENPCScheduleActivity Activity; \}.

  - Neue Property TArray\<FNPCScheduleEntry\> DailySchedule (Kategorie „Schedule").

  - Static-Helfer ScheduleActivityToString() (Enum -\> kleingeschriebener String, den FScheduleSystem versteht).

- ANPCCharacter::LoadTestSchedule():

  - Laedt jetzt bevorzugt aus NPCData-\>DailySchedule ins ECS (FScheduleData).

  - Faellt auf den hardcodierten Test-Plan zurueck, wenn das Array leer ist.

  - Sortiert die Eintraege nach Stunde (Reihenfolge im DataAsset egal).

  - Logs: \[\<Name\>\] Schedule aus DataAsset geladen (N) bzw. … Fallback Test-Plan (N).

##### Bedienung (Editor)

1. Voll rebuilden (neues Enum/Struct/Array – Live Coding reicht nicht).

2. NPC-DataAsset oeffnen (z.B. DA\_Magnus).

3. „Schedule" -\> „Daily Schedule" -\> Eintraege: Hour + Activity (Dropdown).

4. Play.

##### Wissenswert beim Testen

- Blackboard CurrentAction enthaelt den GROSS geschriebenen Enum-Namen (Work/Eat/…), nicht den Schedule-String – BT-Branches entsprechend.

- Needs uebersteuern den Plan (Hunger/Muedigkeit). Zum reinen Ablauf-Testen HungerRate/ExhaustionRate klein halten.

- Weltuhr startet 6:00, RealSecondsPerGameHour = 30. Zum schnellen Testen Wert kleiner setzen oder WorldClock::SetTime(\<Stunde\>).

##### Geaenderte Dateien

- NPC/NPCDataAsset.h (Enum, Struct, Array, Helfer)

- NPC/NPCCharacter.cpp (LoadTestSchedule laedt aus DataAsset)

##### Offen

- LLM-generierter Tagesablauf (GenerateSchedulePrompt) weiterhin nicht verdrahtet – DataAsset-Variante ist jetzt der manuelle Weg.

