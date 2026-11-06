# Devlog – 2026-06-12

Chat-Politur, Bewegungssperre und Verifizierungs-Logs.


# 20-Eintrag: Bewegungssperre bei offenem Chat

##### Ziel

Der Charakter soll sich nicht bewegen, drehen oder springen koennen, solange das Chatfenster offen ist.

##### Vorher (geprueft)

Bewegung war moeglich: ToggleChat() setzt beim Oeffnen FInputModeGameAndUI, wodurch Spiel-Input weiterlaeuft, und Move()/Look() hatten keine Sperre.

##### Umgesetzt (LLM\_NPCCharacter)

- Move() und Look() beginnen jetzt mit if (m\_bChatVisible) return;.

- Neue Wrapper-Funktion DoJump() (prueft m\_bChatVisible, ruft sonst Jump()); das Jump-Started-Binding zeigt jetzt auf DoJump statt direkt auf ACharacter::Jump. Completed bleibt auf StopJumping.

##### Ergebnis

Bei offenem Chat: kein Laufen (WASD), keine Kamera-Drehung, kein Springen. Beim Schliessen (m\_bChatVisible = false) sofort wieder normal.


# 21-Eintrag: Verifizierungs-Logs fuer die Test-Checkliste

##### Ziel

Jeden Punkt der 2-NPC-Checkliste im Output-Log nachvollziehen koennen.

##### Umgesetzt

- ChatWidget (Routing): \[Chat\] Anfrage an NPC '\<Name\>' gesendet (RequestID=…), \[Chat\] Antwort akzeptiert fuer NPC '\<Name\>' (RequestID=…, Server=…), \[Chat\] Antwort IGNORIERT - fremde RequestID=… (Beleg, dass das RequestID-Routing greift).

- ChatWidget (Persistenz): \[Chat\] Verlauf von NPC '\<Name\>' geladen (N Turns).

- NPCCharacter: \[NPC \<Name\>\] Dialog-Turn gespeichert (Player/NPC) | Verlauf: N.

- WorldLocationManager: \[World\] Workstation '\<Tag\>\_i' reserviert von Entity u, … kein freier Platz …, … freigegeben von Entity u (zeigt, dass zwei NPCs dank eindeutiger EntityIDs verschiedene Plaetze bekommen).

##### Hinweis

Bestehende Logs (Threads, ECS-Entity-Erstellung, Hermes→Qwen-Fallback, JSON-Speicherung) decken die uebrigen Checklisten-Punkte ab.


# 22-Eintrag: NPC-Name im Chat und in der gespeicherten JSON

##### Ziel

Im Chatfenster soll der echte Charaktername statt „NPC" stehen, und die gespeicherte Server-Antwort soll festhalten, welcher NPC gesprochen hat.

##### Umgesetzt (Anzeige)

- ChatWidget merkt sich m\_npcDisplayName (aus GetNPCName() in OpenForNPC).

- Verwendet statt „NPC" beim Platzhalter, im Typewriter-Praefix (\[Magnus\]: …) und im wieder geladenen Verlauf. Spielerzeile bleibt „Du".

##### Umgesetzt (JSON)

- Neues Feld FLLMRequest::NPCName, in SendMessage gesetzt und bis in die Worker durchgereicht.

- SaveResponseToFile packt die Server-Antwort jetzt in einen Wrapper und nimmt den Namen in den Dateinamen auf:

- ```
\{  
  "npc": "Magnus",  
  "server": "Hermes",  
  "response": \{ … originale llama-server-JSON … \}  
\}
```

- Datei z.B. Hermes\_Magnus\_20260612\_153012.json. Gilt fuer Hermes und Qwen.

##### Geaenderte Dateien

- LLM\_NPCCharacter.cpp/.h (Bewegungssperre)

- UI/ChatWidget.cpp/.h (Name + Logs)

- NPC/NPCCharacter.cpp (Dialog-Log)

- World/WorldLocationManager.cpp (Workstation-Logs)

- LLM\_Server/LLMRequestManager.cpp/.h (NPCName-Feld, SaveResponseToFile)

- LLM\_Server/Worker/HermesWorker.cpp, QwenWorker.cpp (Name durchreichen)

##### Wichtig

Mehrere Header-/Layout-Aenderungen (neues Struct-Feld, neue Member/Funktionen, geaenderte Signatur) → **voll neu kompilieren** (Editor schliessen, IDE-Rebuild, neu oeffnen). Live Coding uebernimmt das nicht zuverlaessig.

