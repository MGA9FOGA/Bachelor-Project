# Test-Checkliste – Zwei NPCs konfliktfrei (2026-06-09)

Praktischer 2-NPC-Durchlauf im Editor. Abgestimmt auf den aktuellen Code inkl. der zu erwartenden Log-Ausgaben.

> Lesart: Punkte 1–5 sollten **grün** sein. Punkt 6 war ursprünglich der Beleg für den EntityID-Bug — nach dem Fix vom 2026-06-09 sollte auch er grün sein.


## 0. Vorbereitung

- [ ] **Build** läuft ohne Fehler (Live Coding oder Rebuild)

- [ ] **Zwei NPC-Blueprints** im Level mit **unterschiedlichen** NPCData-DataAssets (unterschiedliche WorkTag/HomeTag)

- [ ] ALLMRequestManager-Actor liegt **genau einmal** im Level

- [ ] Beide Server starten → Log \[Hermes\] Ready on port 8080 ✓ **und** \[Qwen\] Ready on port 8081 ✓

## 1. Server & Threads

- [ ] Log: \[LLMRequestManager\] Initialized - Worker Threads started

- [ ] Log: \[HermesWorker\] Thread gestartet + \[QwenWorker\] Thread initialisiert

- [ ] Beim PIE-Stop: beide Server terminieren (kein verwaister llama-server.exe im Task-Manager)

## 2. ECS-Trennung (jeder NPC eigenständig)

- [ ] Log bei BeginPlay: **zwei** \[ECS\] … initialized Zeilen mit den **richtigen Namen**

- [ ] **Nach dem Fix:** die zwei NPCs loggen **unterschiedliche** Entity-IDs (z.B. ID 1 und ID 2), nicht beide ID 1

## 3. Dialog-Routing (Kern-Fix)

- [ ] NPC **A** ansprechen → Chat öffnet mit **A's** Prompt/Persönlichkeit

- [ ] Nachricht senden → Antwort erscheint im Chat, „…" wird ersetzt

- [ ] Weglaufen, **NPC B** ansprechen → Chat ist **geleert** und zeigt **B's** Persönlichkeit, nicht A's

- [ ] **Kritischer Test:** Während A's Antwort noch lädt, schnell zu B wechseln → B's Chat zeigt **keine** Antwort, die an A ging

- [ ] Log: nur **der angesprochene** NPC loggt \[Name\] Reply: … / Intent: … | Mood: … — nicht beide

## 4. ECS-Auswirkung bleibt lokal

- [ ] Nach Gespräch mit A: nur **A's** Emotion/Memory ändert sich (per DebugPrintECS), B bleibt unberührt

- [ ] B reagiert **nicht** auf A's Antwort

## 5. Fallback & Last

- [ ] Hermes manuell killen → nächste Anfrage geht an Qwen, Log: \[HermesWorker\] HTTP … – leite zu Qwen weiter + \[LLM\] Request -\> Qwen (Fallback)

- [ ] Antwort kommt trotzdem im Chat an (ServerUsed = Qwen)

## 6. Workstation-Reservierung (war der EntityID-Bug)

- [ ] Beide NPCs mit **gleichem WorkTag** zur Arbeit schicken

- [ ] **Nach dem Fix:** sie reservieren **unterschiedliche** Arbeitsplätze (stehen nicht mehr aufeinander)

- [ ] Wenn NPC A den Platz freigibt, behält NPC B seinen Platz

## 7. Sauberkeit

- [ ] Keine Crashes beim wiederholten Öffnen/Schließen des Chats (Typewriter-Timer)

- [ ] Saved/LLMResponses/ füllt sich mit JSON-Logs (eine Datei pro Antwort)

- [ ] Keine „LLMRequestManager nicht gefunden!"-Meldung im Chat

