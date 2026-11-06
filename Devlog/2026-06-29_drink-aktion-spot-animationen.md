# Devlog – 2026-06-29

Seit dem großen Spots-/Needs-Block: erst eine Schlaf-Garantie, damit wirklich
jeder NPC ins Bett geht, dann `drink` als eigene Tätigkeit an den Essplätzen,
spotabhängige Animationen je nach Aktion und zum Schluss ein Aufräumen, das die
ganze Anim-Steuerung typsicher (Enums statt Strings) macht und ans
Animation-Blueprint anbindet.
(Fortsetzung nach #59.)

---

# 60-Eintrag: Schlaf-Garantie – kein NPC arbeitet mehr durch

##### Problem
Seit das LLM Schlafen/Aufwachen selbst plant, kamen vereinzelt Tagespläne (vor
allem von Qwen) **ganz ohne `sleep`-Eintrag** zurück, z. B.
`{6:work, 8:work, 12:eat, 14:drink, 18:work}`. Solche NPCs arbeiteten die ganze
Nacht durch – der erschöpfungsbasierte Override greift nur ab `Exhaustion ≥ 0.7`,
was bei Steh-Tätigkeiten praktisch nie passiert.

##### Umgesetzt
- In `LoadScheduleFromLLM`: nach dem Parsen prüfen, ob **irgendein** Eintrag
  `sleep` ist. Falls nicht, wird einer zur **typgerechten Schlafstunde**
  (`SleepHourForType`) ergänzt.
- Plant das LLM selbst Schlaf, bleibt der unangetastet – es entscheidet die Zeit.
- Log: `Kein Schlaf im LLM-Plan – Schlaf um 21:00 ergaenzt`.

##### Hinweis
Gleichgetypte NPCs schlafen damit zur selben Stunde. Wer das staffeln will, gibt
ihnen unterschiedliche `SleepType`s.

---

# 61-Eintrag: `drink` als eigene Aktion (an den Essplätzen)

##### Ausgangslage
Das Spiel kannte nur `work, eat, rest, sleep, wander, idle`. Plante das LLM
`drink` oder `tell stories`, wurde das stur auf `wander` normalisiert – `drink`
wurde also zu „umherschlendern“, obwohl Trinken eher in die Taverne gehört.

##### Umgesetzt
- Neuer Wunsch `ENPCDesire::Drink`.
- `ActivityStringToDesire`: `"drink"` → `Drink`; `drink` in die `validActions`
  aufgenommen und im Schedule-Prompt + Beispiel-JSON erlaubt.
- `Drink` reserviert **denselben Essplatz** wie `Eat`
  (`ChooseNextDestination`: `goal == Eat || goal == Drink` → `ESpotType::Eat`).
- Bewusst **kein** eigener Durst-Bedarf: Trinken ist eine geplante Tätigkeit/Geste,
  kein neuer Need-Wert. (`tell stories` bleibt vorerst `wander` – echtes
  Storytelling ist ein späteres Feature.)

---

# 62-Eintrag: Aktionsabhängige Spot-Animationen

##### Ziel
Ein Spot soll **je nach Tätigkeit** eine andere Animation spielen können – z. B.
am Essplatz beim Essen eine andere als beim Trinken.

##### Umgesetzt
- `AWorldSpot` bekam neben der Standard-Animation zwei optionale Overrides:
  - `ActionAnimStates` (loopende Pose) und `ActionMontages` (One-Shot),
    jeweils **pro Tätigkeit** (`ENPCDesire`).
  - `GetAnimStateForAction()` / `GetMontageForAction()` liefern den Override für die
    Aktion, sonst den **Standard** des Spots (Fallback = Sicherheitsnetz: nie „NPC
    steht regungslos“).
- NPC und BT übergeben einfach ihr aktuelles `goal` – der Spot entscheidet die Pose.

##### Editor
Am Spot unter *Spot|Animation*: Standard setzen reicht für Ein-Zweck-Spots (Bett,
Amboss, Bank). Nur Mehrzweck-Spots (Essplatz: `Eat` vs. `Drink`) befüllen die Maps.

---

# 63-Eintrag: Typsicherheit – Enums statt Strings, AnimBP-Anbindung

##### Problem
Die aktionsabhängigen Anims waren zuerst über **Strings** (`FName`-Tags)
verschlüsselt – tippfehleranfällig (`"drnik"` → stiller Fallback). Und der alte
`AnimTag` war faktisch tot: er wurde nur geloggt, war nicht `BlueprintCallable`,
und die eigentliche Animation lief allein über die Montage.

##### Umgesetzt
- **`ENPCDesire` zentralisiert**: aus dem schweren `NPC_ECS.h` ins gemeinsame
  `NPCWorldEnums.h` verschoben, damit auch `World/` es ohne dicke Abhängigkeit
  nutzen kann. Map-Schlüssel sind jetzt das Enum (Dropdown im Editor).
- **Neues `EAnimState`-Enum** (`None, Idle, Eat, Drink, Sleep, Work, Rest`) ersetzt
  den String-Tag. Die loopende Pose ist damit typsicher.
- `GetCurrentAnimState()` ist jetzt **`BlueprintPure`** – das Animation-Blueprint
  liest den Wert und schaltet die passende State-Machine-Pose. Die Montage bleibt
  als ergänzender One-Shot erhalten.
- String-Helfer `DesireToActionName` und der tote `m_currentAnimTag` entfernt.

##### AnimBP-Anbindung (neu nötig)
Im Animation-Blueprint: Variable vom Typ `EAnimState`, in *Update Animation* aus
`NPCCharacter::GetCurrentAnimState()` befüllen, und die State-Machine-Übergänge
darauf legen (`Eat/Drink/Sleep/…`; `None` → Locomotion/Idle).

##### Build
Voller Rebuild nötig (neues/verschobenes UENUM → UHT regeneriert die Reflection).

---

# Aktueller Stand
Jeder NPC schläft verlässlich (notfalls per ergänztem Schlaf-Eintrag), `drink`
ist eine echte Tätigkeit an den Essplätzen, und Spots können pro Aktion eigene
Posen spielen. Die gesamte Anim-Steuerung ist jetzt enum-basiert und über
`GetCurrentAnimState()` sauber ans Animation-Blueprint angebunden – Montagen
bleiben für One-Shots erhalten.
