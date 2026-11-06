# Devlog – 2026-06-25 (Teil 2)

Tag/Nacht-Beleuchtung mit Sonne **und** Mond, Chat nur noch bei anvisiertem NPC, Schlafplatz-Fallback in die Taverne und ein Schlaf-Typ-System, das die feste Schlafzeit über dem LLM-Plan erzwingt. (Fortsetzung nach \#45.)


# 46-Eintrag: Tag/Nacht-Beleuchtung (Sonne + Mond)

##### Ziel

Helligkeit und Farbe der „Sonne" sollen sich an die Uhrzeit anpassen; abends kommt der Mond raus und gibt bläulich-türkises Licht.

##### Umgesetzt

- Neuer Actor **ADayNightController** (World/), liest jeden Frame die UWorldClock und berechnet die **Sonnenhöhe** (elevation = sin((hour-6)/12 \* PI): +1 Mittag, 0 Horizont, -1 Mitternacht).

- **Sonne** (DirectionalLight, im Editor zugewiesen):

  - **Helligkeit** nachts NightIntensity → mittags DayIntensity, weicher Übergang um Sonnenauf-/untergang.

  - **Farbe** Nacht (kühl/blau) → Horizont (warm/orange) → Mittag (neutralweiß).

  - **Sonnenstand** (optional bDriveSunRotation): 6 Uhr Horizont, 12 Uhr von oben, 18 Uhr Horizont.

- **Mond** (zweite DirectionalLight, optional):

  - Kommt abends raus (moonFactor), tagsüber komplett aus (SetVisibility).

  - **Farbe** MoonColor default bläulich-türkis (0.2, 0.6, 0.85).

  - Mondbahn = Sonnenbahn um 12 h versetzt.

- Alle Werte/Farben im Editor einstellbar.

##### Editor-Setup

- DayNightController ins Level, SunLight + MoonLight zuweisen.

- **Wichtig:** Mobility der Lights auf **Movable** (sonst keine Laufzeit-Änderung).

- Sichtbare Mondscheibe: Mond-Light dem SkyAtmosphere als zweites Licht geben (*Atmosphere Sun Light Index = 1*).


# 47-Eintrag: Chat nur bei anvisiertem NPC

##### Problem

Der Chat ging per Interact-Taste **immer** auf – auch wenn kein NPC in der Nähe war (ToggleChat machte das Widget bedingungslos sichtbar, der Interact()- Check kam zu spät).

##### Umgesetzt

- ToggleChat prüft beim **Öffnen** über das Interact-Interface (GetNearbyInteractable() + CanInteract()): ohne anvisierten, interagierbaren NPC bleibt der Chat zu.

- Schließen ist weiter immer erlaubt (man kann ja weglaufen).


# 48-Eintrag: Schlafplatz-Fallback in die Taverne

##### Umgesetzt

- Beim Schlafen reserviert der NPC zuerst ein Bett im eigenen Zuhause (HomeTag).

- **Kein Bett frei → Taverne** (Tavern-Tag) nach freien Betten prüfen.

- Erst wenn auch dort voll → Ortsmitte als letzter Fallback.

- Log: Kein Bett zuhause frei – schlaeft in der Taverne.


# 49-Eintrag: Schlaf-Typ – feste Schlafzeit über dem LLM-Plan

##### Problem

„Aldric geht nie schlafen." Ursache war doppelt:

1. Schlaf-Override triggerte nur bei Exhaustion ≥ 0.7, und Erschöpfung steigt nur beim Laufen/Arbeiten → Steh-NPCs wurden nie müde genug.

2. Vor allem: bUseLLMSchedule **überschrieb den von Hand gesetzten** **DataAsset-Plan** komplett. Enthielt der LLM-Plan kein sleep, schlief der NPC nie.

##### Entscheidung

Das LLM soll weiter den Tag planen, **die Schlafzeit aber fest** – abhängig vom Charakter-Typ über ein Enum im DataAsset.

##### Umgesetzt

- Neues Enum **ENPCSleepType** (im DataAsset gesetzt):

| Typ | schläft | wacht |
| - | - | - |
| Frühaufsteher | 21:00 | 5:00 |
| Normal (Default) | 22:00 | 6:00 |
| Langschläfer | 0:00 | 8:00 |
| Nachtmensch | 2:00 | 9:00 |


- Zentrale Helfer SleepHourForType / WakeHourForType.

- LoadScheduleFromLLM **erzwingt die Schlafphase**: alle LLM-Einträge im Schlaffenster (wrap um Mitternacht) raus, dann fester sleep-Eintrag rein.

- FNeedsData::SleepHourThreshold / WakeHourThreshold werden beim Init auf den Typ gesetzt → auch die erschöpfungsbasierte Sleep/Rest-Wahl passt zum Typ.

- Log: Schlafzeit erzwungen: 22:00 Uhr (Typ-Fenster bis 06:00).

##### Hinweis

Voraussetzung bleibt ein erreichbares Bett (AWorldSpot, Type=Sleep) – sonst greift der Taverne-Fallback (\#48).


# Nebenbei

- RealSecondsPerGameHour auf 2.0 gesetzt (Zeitraffer zum Testen: 1 Tag ≈ 48 s).


# Aktueller Stand

Die Welt hat einen sichtbaren Tag/Nacht-Zyklus (Sonne wandert, färbt sich, abends bläulich-türkiser Mond), der Chat öffnet nur noch bei einem anvisierten NPC, und jeder NPC schläft verlässlich zu seiner typgerechten Zeit – das LLM plant den Rest des Tages drumherum. Kein Bett zuhause? → Taverne.

