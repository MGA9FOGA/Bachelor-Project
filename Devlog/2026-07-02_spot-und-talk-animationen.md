# Devlog – 2026-07-02

Großer Animations-Block: NPCs bekommen echte Körper-Animationen. Locomotion über
eine C++-AnimInstance, Tätigkeits-Animationen an Spots als kombinierte Montage
(Hinsetzen → Sitz-Loop → Aufstehen), und eigene Talk-Animationen fürs Gespräch.
Dazu ein paar Kopplungen an die Simulation (Needs, Rest-Dauer, Positions-Offset).
(Fortsetzung nach #63.)

---

# 64-Eintrag: NPC-Locomotion über eine C++-AnimInstance

##### Ziel

Die Animationen sollen sauber aus dem Zustand des NPCs kommen – ohne die ganze
Logik ins Animation-Blueprint zu quetschen.

##### Umgesetzt

- Neue Klasse **UNPCAnimInstance** (NPC/Animation/). `NativeUpdateAnimation` liest
  jeden Frame den Pawn und stellt dem AnimBP fertige Variablen bereit:
  - `GroundSpeed` (horizontale Geschwindigkeit) → 1D-Locomotion-Blendspace,
  - `bIsMoving`, `bIsInAir`.
- Das AnimBP erbt von dieser Klasse (Class Settings → Parent Class) und macht nur
  noch den visuellen Teil: **Blendspace (Idle→Walk→Run) → Slot 'DefaultSlot' → Output**.
- Die Tätigkeits-Animationen laufen NICHT über eine Posen-State-Machine, sondern
  als Montagen über den `DefaultSlot` – das AnimBP bleibt dadurch minimal.

##### Hinweis

Ohne `Slot 'DefaultSlot'` im AnimGraph sind alle Montagen unsichtbar. Diagnose-Logs
in `PlaySpotAnimation` melden fehlenden Slot / falsches Skelett (Montage-Länge 0).

---

# 65-Eintrag: Spot-Animationen als EINE kombinierte Montage

##### Ausgangslage

Zuerst als drei getrennte Montagen am Spot gebaut (Intro/Loop/Outro). Der Wechsel
zwischen zwei Montagen erzeugte aber immer wieder einen **1-Frame-Basispose-Blitz**
(Loop wird gestoppt, bevor das Outro Gewicht hat). Diverse Zwischenschritte
(Blending-Out-Delegate, `bStopAllMontages=false`) halfen bei Intro→Loop, aber
Loop→Outro blieb unsauber.

##### Umgesetzt (Plan B)

- **Eine** `SpotMontage` pro Spot mit drei Sections:
  - `Intro` (Stehen→Sitzen) → Next: Loop
  - `Loop` (Tätigkeit) → Next: **Loop** (loopt auf sich selbst)
  - `Outro` (Sitzen→Stehen) → Next: None
- Ankommen: Montage von vorne (Intro→Loop). Verlassen: `Montage_JumpToSection("Outro")`.
- Da es **derselbe** Montage-Instanz bleibt, gibt es **keinen Montage-Wechsel** →
  technisch keine Lücke mehr.
- Section-Namen am Spot einstellbar (`IntroSection`/`OutroSection`).
- Verschiedene Spots können verschiedene `SpotMontage`s haben (z. B. zwei
  Arbeitsplätze mit unterschiedlichen Anims).

##### Verworfen

Das enum-basierte Posen-System aus #62/#63 (`EAnimState`, `PoseOverride`) wurde
komplett entfernt – ein Enum-Wert konnte verschiedene Spot-Animationen nicht
unterscheiden, und Montagen erlauben Intro/Loop/Outro sauberer.

---

# 66-Eintrag: Bewegung wartet aufs Outro

##### Problem

Der NPC lief los, bevor er fertig aufgestanden war (Outro und Weglaufen
überlappten sich).

##### Umgesetzt

- `PlaySpotOutro()` springt in die Outro-Section, `IsPlayingOutro()` prüft, ob die
  aktuelle Section noch das Outro ist.
- In `BTTask_PerformAction` bleibt die Aktion **InProgress**, solange
  `IsPlayingOutro()` true ist → der NPC steht erst ganz auf, **dann** läuft er.
- Latch `m_bOutroPlayed`: das Outro wird pro Spot-Besuch nur einmal ausgelöst
  (TickTask läuft jede Frame). Kann das Outro nicht spielen → kein Hänger.

---

# 67-Eintrag: Needs zählen erst mit aktiver Loop-Animation

##### Ziel

Erholung/Arbeit soll erst zählen, wenn der NPC WIRKLICH sitzt (Loop läuft) – nicht
schon beim Hinlaufen oder während der Hinsetz-Animation.

##### Umgesetzt

- Neues Flag `FWaypointData::bLoopActive`, vom Character gesetzt.
- Ein Timer über die Länge der Intro-Section schaltet `bLoopActive` erst frei, wenn
  der Loop beginnt (kein Intro → sofort).
- Needs-System: `bAtSpot = bIsPausing && bLoopActive` – am Spot zählt jetzt nichts
  mehr während Intro/Hinweg.

---

# 68-Eintrag: Intro-Offset gegen Clip-Verschiebung

##### Problem

Mixamo-„sit down"-Clips treten oft zurück – der NPC setzte sich „weiter hinten" als
Loop/Outro. Root Motion sah falsch aus.

##### Umgesetzt

- Neues Feld `AWorldSpot::IntroMeshOffset` (FVector, lokal zur Blickrichtung).
- Wird NUR während der Intro-Section aufs Mesh gelegt (`AddRelativeLocation`) und
  beim Loop wieder sauber zurückgenommen (auch beim Verlassen/nächsten Spot).
- Gleicht eine konstante Clip-Verschiebung aus, ohne Root Motion.

---

# 69-Eintrag: Rest-Dauer verlängert

##### Problem

Ausruhen war viel zu kurz: Erschöpfung fiel bei `RestRecoveryRate = 2.0` in ~0.25
Spielstunden unter die Sättigungsschwelle – im Zeitraffer quasi sofort.

##### Umgesetzt

- `RestRecoveryRate` 2.0 → **0.5** (der NPC sitzt deutlich länger).
- `WanderRestRecoveryRate` 0.6 → 0.2 (bleibt langsamer als die Bank).

---

# 70-Eintrag: Talk-Animationen (Rede-Idle + Antwort)

##### Ziel

Während eines Gesprächs eine Rede-Idle, und eine eigene Animation, während der NPC
gerade antwortet.

##### Umgesetzt

- Zwei Montagen am NPC: `TalkIdleMontage` (ganzes Gespräch) und `TalkReplyMontage`
  (während der NPC antwortet).
- **Zustands-Loop** (`m_bTalkReplying`): `PlayCurrentTalkMontage()` spielt die
  Montage zum aktuellen Zustand und startet sie beim Ausblenden selbst neu → loopt
  im Code (Montage muss NICHT auf Loop stehen).
- Kopplung:
  - `SetTalking(true/false)` → Rede-Idle starten / alles stoppen.
  - ChatWidget: Typewriter-Start → `PlayTalkReply()`, Typewriter-Ende →
    `EndTalkReply()`. Die Antwort-Animation läuft also genau so lange wie das Tippen.
- Sicherung: spielt eine Montage nicht (Länge 0), wird nicht geloopt → kein Hänger.

---

# Aktueller Stand

NPCs laufen (Blendspace über `NPCAnimInstance`), setzen sich an Spots über eine
lückenlose Intro→Loop→Outro-Montage hin, stehen erst ganz auf, bevor sie weglaufen,
und erholen sich erst, wenn sie wirklich sitzen. Sitz-Position gleicht ein
Intro-Offset aus, die Rest-Dauer ist spürbar. Im Gespräch läuft eine Rede-Idle, die
nur fürs Antworten unterbrochen wird. Der `Slot 'DefaultSlot'` im AnimBP ist die
gemeinsame Voraussetzung für alle Montagen.
