# Devlog – 2026-06-25

Der Tagesablauf wird jetzt nicht mehr nur einmal beim Start erzeugt, sondern **dynamisch neu geplant** – täglich und wenn der NPC etwas Neues erfährt. (Fortsetzung nach \#44.)


# 45-Eintrag: Dynamische Neuplanung des Tagesablaufs

##### Ausgangslage

Der LLM-Plan wurde bisher **einmal im BeginPlay** generiert und galt dann für immer (gleicher Plan an jedem Spieltag). Neue Infos – z. B. aus einem Gespräch – hatten keinen Einfluss auf den Ablauf.

##### Umgesetzt

**1. Tageszähler in der Weltuhr**

- UWorldClock::GetDay() – zählt hoch, sobald die Stunde von 23 auf 0 springt.

- NPC merkt sich m\_lastScheduleDay; im Tick: neuer Tag → RequestLLMSchedule().

- → Jeder Spielmorgen ein frischer, an den aktuellen Zustand angepasster Plan.

**2. Öffentlicher Event-Hook ReplanSchedule()**

- BlueprintCallable – von C++ **und** Blueprint aufrufbar.

- Guard: tut nichts, wenn bUseLLMSchedule aus ist.

- Wiederholte Aufrufe sind safe (AddDynamic ist idempotent, Antworten werden per RequestID gefiltert).

**3. „NPC erfährt etwas" → OnLearnedInformation(info, importance, source)**

- Legt die Info ins Gedächtnis (FMemoryData) und plant neu.

- Ein Konzept für **beide Quellen**: Spieler-Gespräch *und* (später) NPC-zu-NPC-Infoaustausch:

- ```
otherNPC-\>OnLearnedInformation(TEXT("Markt heute am Hauptplatz"), 0.7f, this);
```

**4. Quest-Intent löst Neuplanung aus**

- In ApplyDialogueResult: bei EIntentType::Quest → ReplanSchedule().

- Die Dialog-Antwort liegt bereits im Gedächtnis und fließt damit in den neuen Plan ein. Greeting/Trade/Neutral lösen bewusst **keine** Neuplanung aus (sonst LLM-Call bei jedem Smalltalk).

**5. Plan-Prompt nutzt das Gelernte (der entscheidende Teil)**

- GenerateSchedulePrompt() hängt jetzt die **3 wichtigsten Erinnerungen** an (Recent things you learned (take these into account)).

- Ohne das würde die KI bei jeder Neuplanung denselben Plan erzeugen – jetzt arbeitet sie neue Infos in den Tagesablauf ein.

##### Bewusst weggelassen

- **Keine Gefahren/Safety-Trigger** (auf Wunsch – im aktuellen Spiel gibt es noch keine Bedrohungen). Der FeelsUnsafe()-Pfad bleibt ungenutzt, lässt sich aber jederzeit über denselben Hook anhängen.

##### Logs zum Prüfen

- Neuer Spieltag (N) – plane Tagesablauf neu

- Quest-Intent – plane Tagesablauf neu

- Neue Info gelernt: "…" (Wichtigkeit X%) – plane neu

- danach wie gehabt: LLM-Schedule angefragt → LLM-Schedule uebernommen (N)

##### Offen / Nice-to-have

- Spieler-**Wortlaut** zusätzlich als Memory ablegen (aktuell nur die NPC-Antwort "I replied: …"), damit der Auftrag wortgenau in den Plan kommt.

- NPC-zu-NPC-Infoaustausch (Gossip), der OnLearnedInformation füttert.

- Optional: Neuplanung nicht um Mitternacht, sondern zur Aufwach-Stunde.


# Aktueller Stand

NPCs leben einen LLM-generierten Tagesablauf, der sich **mit der Welt** **weiterentwickelt**: jeden Spielmorgen neu, und sofort wenn der Spieler im Gespräch einen Auftrag gibt (Quest-Intent) oder der NPC anderweitig etwas erfährt. Das Gelernte fließt über das Gedächtnis in den Plan-Prompt ein.

